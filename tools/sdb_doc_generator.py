#! /usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (c) 2018, SequoiaDB and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

import os
import sys
import re
import optparse



ERR_OK = 0
ERR_INVALID_ARG = 1
ERR_PARSE = 2

OUT_FILE_NAME = 'sdb_doc'

DESCRIPTION = '''sdb_doc_generator - generates the default configure or \
document from the source code FILE('../src/sequoiadb/handler/ha_sdb_conf.cc' by default).
'''

USAGE = "Usage: %prog [OPTIONS] [FILE]"
EXTENDED_HELP = '''\
Exit status:
 0  if OK,
 1  if invalid arguments,
 2  if failed to parse document.
'''

MY_CNF_DEFAULT = '''\
[client]\n
default_character_set=utf8mb4\n
\n
[mysqld]\n
max_connections=1024\n
max_prepared_stmt_count=128000\n
sql_mode=STRICT_TRANS_TABLES,ERROR_FOR_DIVISION_BY_ZERO,NO_AUTO_CREATE_USER,\
NO_ENGINE_SUBSTITUTION\n
character_set_server=utf8mb4\n
collation_server=utf8mb4_bin\n
lower_case_table_names=1\n
# Prioritizes cached table statistics over direct stats collecting for SequoiaDB \
engine tables when querying information_schema.tables.
# information_schema_tables_stats_cache_first=OFF
'''

MY_CNF_DEFAULT_STORAGE = "\ndefault_storage_engine=SequoiaDB\n"

MYSQL_CNF_APPEND = '''
join_buffer_size=2097152\n
optimizer_switch=index_merge_intersection=off,batched_key_access=on
'''

MARIADB_CNF_APPEND = '''
join_cache_level=8\n
optimizer_switch=mrr=on,mrr_cost_based=off,join_cache_incremental=on,\
join_cache_hashed=on,join_cache_bka=on,optimize_join_buffer_size=on,\
index_merge_intersection=off
'''

MY_OPTIMIZER_OPTIONS = 'optimizer_options'

is_mariadb = False

def enum(*args):
    enums = dict(zip(args, range(len(args))))
    return type('Enum', (), enums)

FormatType = enum('MARKDOWN', 'CNF', 'ALL')
Language = enum('ENGLISH', 'CHINESE')

class DocParser(optparse.OptionParser):
    def print_help(self, output=None):
        optparse.OptionParser.print_help(self, output)
    def format_epilog(self, formatter):
        return self.epilog if self.epilog is not None else ''
        
class DocTuple:
    def __init__(self):
        self.name = ""
        self.type = ""
        self.default = ""
        self.online = ""
        self.scope = ""
        self.desp_cn = ""
        self.desp_en = ""

    def __repr__(self):
        return repr((self.name, self.type, self.default, self.online,
                     self.scope, self.desp_cn, self.desp_en))

    @staticmethod
    def get_md_header(language):
        if language == Language.CHINESE:
            header = '|参数名|类型|默认值|动态生效|作用范围|说明|'
        elif language == Language.ENGLISH:
            header = '|Parameter|Type|Default|Online|Scope|Description|'
        else:
            print("ERROR: Unknown language:" + fmt)
            return None
        header += '\n|---|---|---|---|---|---|\n'
        return header

    def toString(self, fmt = FormatType.MARKDOWN, language = Language.ENGLISH):
        if fmt == FormatType.MARKDOWN:
            if language == Language.CHINESE:
              descript = self.desp_cn
            elif language == Language.ENGLISH:
              descript = self.desp_en
            return "|" + self.name + "|" + self.type + "|" + self.default + \
                    "|" + self.online + "|" + self.scope + "|" + descript + "|\n"
        elif fmt == FormatType.CNF:
            default_val = self.default
            last = len(default_val) - 1
            if (self.default[0] == '"' and self.default[last] == '"') or \
                (self.default[0] == "'" and self.default[last] == "'"):
                default_val = default_val[1:last]
            res = "# " + self.desp_en + "\n"
            res += "# " + self.name + "=" + default_val + "\n\n"
            return res
        else:
            print("ERROR: Unknown format:" + fmt)
            return None

class DocExtractor:
    def __init__(self, language):
        self.language = language
        self.tuples = []

    def is_alpha(self, character):
        return ('A' <= character and character <= 'Z') or \
                ('a' <= character and character <= 'z')

    def get_tuple(self, declare):
        # Declare format:
        # static MYSQL_XXXVAR_XXX(name, varname, opt,
        #     "<English Description>"
        #     "(Default: <Default Value>)."
        #     /*<Chinese Description>*/,
        #     check, update, def);

        # Get `scope`
        t = DocTuple()
        bgn = 0
        while declare[bgn] != '_':
            bgn += 1
        bgn += 1
        end = bgn
        while declare[end] != '_':
            end += 1
        scope_type = declare[bgn:end]
        if scope_type == 'SYSVAR':
            t.scope = 'Global'
        elif scope_type == 'THDVAR':
            t.scope = 'Global, Session'
        else:
            print("ERROR: Invalid scope declare:" + scope_type)
            return None

        # Get `type`
        bgn = end + 1
        end = bgn + 1
        while self.is_alpha(declare[end]):
            end += 1
        data_type = declare[bgn:end]
        if data_type == 'BOOL':
            t.type = 'bool'
        elif data_type == 'STR':
            t.type = 'string'
        elif data_type == 'INT':
            t.type = 'int'
        elif data_type == 'UINT':
            t.type = 'unsigned int'
        # Linux64, gcc, long is 64bit
        elif data_type == 'LONG' or data_type == 'LONGLONG':
            t.type = 'long'
        elif data_type == 'ULONG' or data_type == 'ULONGLONG':
            t.type = 'unsigned long'
        elif data_type == 'ENUM':
            t.type = 'enum'
        elif data_type == 'SET':
            t.type = 'set'
        elif data_type == 'DOUBLE':
            t.type = 'double'
        else:
            print("ERROR: Unknown type declare:" + data_type)
            return None

        # Get `name`
        bgn = end + 1
        while not self.is_alpha(declare[bgn]):
            bgn += 1
        end = bgn
        while declare[end] != ',':
            end += 1
        name = declare[bgn:end]
        t.name = "sequoiadb_" + name

        # Get `online`
        bgn = end + 1
        # Skip parameter varname, which is only belong to SYSVAR
        if (scope_type == 'SYSVAR'):
            while declare[bgn] != ',':
                bgn += 1
        while not self.is_alpha(declare[bgn]):
            bgn += 1
        end = bgn + 1
        while declare[end] != ',':
            end += 1
        opt = declare[bgn:end]
        if opt.find("PLUGIN_VAR_READONLY") > 0:
            t.online = "No"
        else:
            t.online = "Yes"

        # Get `default`
        comment = ""
        bgn = end + 1
        while True:
            while declare[bgn] != '"':
                bgn += 1
            bgn += 1
            end = bgn
            while declare[end] != '"' or declare[end - 1] == '\\':
                end += 1
            comment += declare[bgn:end]

            # find next adjacent string
            bgn = end + 1
            while declare[bgn] == ' ' or declare[bgn] == '\n':
                bgn += 1
            if declare[bgn] != '"':
                break

        comment = comment.replace('\\"', '"')
        default_declare = re.search("\(\s?Default\s?:.*\)", comment)
        if default_declare:
            default_val = default_declare.group(0)
            val_bgn = 0
            while default_val[val_bgn] != ':':
                val_bgn += 1
            val_bgn += 1
            while default_val[val_bgn] == ' ':
                val_bgn += 1
            val_end = len(default_val) - 2
            while default_val[val_end] == ' ':
                val_end -= 1
            val_end += 1
            t.default = default_val[val_bgn:val_end]
        else:
            print("WARN: No default value in " + t.name)
            t.default = '-'

        if name == MY_OPTIMIZER_OPTIONS:
            # Remove space
            t.default = t.default.replace(' ', '')

        # Get `desp_en`
        if default_declare:
            desp_end = comment.find(default_declare.group(0))
        else:
            desp_end = len(comment)
        t.desp_en = comment[0:desp_end]

        # Get `desp_cn`
        while declare[bgn] != '/' or declare[bgn + 1] != '*':
            bgn += 1
        bgn += 2
        end = bgn
        while declare[end] != '*' or declare[end + 1] != '/':
            end += 1
        t.desp_cn = declare[bgn:end]

        return t

    def get_tuples(self, conf_src_path):
        if len(self.tuples) > 0:
            return self.tuples
        with open(conf_src_path, 'r') as f:
            skip_next = False
            while True:
                line = f.readline()
                if not line:
                    break
                if re.match(r'^//\s?SDB_DOC_OPT\s?=\s?IGNORE.*$', line):
                    skip_next = True
                if re.match(r'^static MYSQL_...VAR_.*\(.*$', line):
                    declare = line
                    while not re.match(r'^.*\);$', line):
                        line = f.readline()
                        if not line:
                            break
                        declare += line
                    if not skip_next:
                        t = self.get_tuple(declare)
                        if not t:
                            return None
                        self.tuples.append(t)
                    else:
                        skip_next = False
        return self.tuples;

class DocExporter:
    def __init__(self, language, fmt):
        self.language = language
        self.fmt = fmt

    def get_file_path(self, out_dir, fmt):
        if fmt == FormatType.MARKDOWN:
            suffix = '.md'
        elif fmt == FormatType.CNF:
            suffix = '.cnf'

        if not os.path.isdir(out_dir):
            print("WARN: Specified output directory is invalid. Using "
                  "current directory.")
            out_dir = DFT_ARG_OUT

        return out_dir + '/' + OUT_FILE_NAME + suffix

    def export(self, tuples, out_dir, output_contex):
        if self.fmt == FormatType.MARKDOWN or self.fmt == FormatType.ALL:
            path = self.get_file_path(out_dir, FormatType.MARKDOWN)
            with open(path, 'w') as f:
                tuples = sorted(tuples, key = lambda tup: tup.name)
                f.write(DocTuple.get_md_header(self.language))
                for t in tuples:
                    f.write(t.toString(FormatType.MARKDOWN, self.language))

        if self.fmt == FormatType.CNF or self.fmt == FormatType.ALL:
            path = self.get_file_path(out_dir, FormatType.CNF)
            with open(path, 'w') as f:
                f.write(output_contex)
                f.write("\n")
                for t in tuples:
                    f.write(t.toString(FormatType.CNF))
#    pdb.set_trace()
def main():
    global is_mariadb
    # Setup the command parser
    program = os.path.basename(sys.argv[0]).replace(".py", "")
    parser = DocParser(
        description = DESCRIPTION,
        usage = USAGE,
        epilog = EXTENDED_HELP,
        prog = program
    )
    #Add --language option
    parser.add_option("-l", "--language", action='store', type="string",
                      default='cn', dest="language",
                      help="Document language. Options: cn(Chinese), "
                           "en(English).")
    #Add --out option
    parser.add_option("-o", "--out", action='store', type="string",
                      default='.', dest="output",
                      help="Output directory. Use current directory "
                           "by default.")
    
    #Add --format option
    parser.add_option("-f", "--format", action='store', type="string",
                      default='all', dest="format",
                      help="Document format. Options: md(MarkDown),"
                           "cnf(my.cnf format), all(All above). Default: all." )
    #Add --project-type option
    parser.add_option("-t", "--project-type", action='store', type="string",
                      default='mysql', dest="project_type",
                      help="Project type. Options: mysql, mariadb. "
                           "Default: mysql")
                           
    #Add --built-in option
    parser.add_option("-b", "--built-in", action='store', type=int,
                      default=1, dest="built_in",
                      help="Document output based on built-in storage or not. "
                           "1: built-in sequoiadb storage. "
                           "0: storage install manually."
                           "Default: 1")

    opt, args = parser.parse_args()         
    if opt.language == 'cn':
        language = Language.CHINESE
    elif opt.language == 'en':
        language = Language.ENGLISH
    else:
        print("ERROR: Invalid language option. Please use 'cn' or 'en'")
        sys.exit(ERR_INVALID_ARG)
    
    if opt.format == 'all':
        format = FormatType.ALL
    elif opt.format == 'md':
        format = FormatType.MARKDOWN
    elif opt.format == 'cnf':
        format = FormatType.CNF
    else:
        print("ERROR: Invalid format option. Please use 'md', 'cnf' or 'all'")
        sys.exit(ERR_INVALID_ARG)
        
    if not opt.output:
       output_dir = default_output_dir
    else:
       output_dir=opt.output
       
    if os.path.isdir(output_dir) == 0:
        print("ERROR: Output directory '{dir}' is not a directory."
              .format(dir=output_dir))
        sys.exit(ERR_INVALID_ARG)
        
    # Get directory of this script.
    if len(args) > 1:
        print("ERROR: Invalid FILE argument. Please input correct source code"
              " FILE path.")
        sys.exit(ERR_INVALID_ARG)

    if not args:
        print("Using source code FILE '../src/sequoiadb/handler/ha_sdb_conf.cc' "
              "by default.")
        current_dir = os.path.abspath(os.path.dirname(__file__))
        project_dir = os.path.join(current_dir, "../")
        conf_src_path = os.path.join(project_dir, "src/sequoiadb/handler/ha_sdb_conf.cc")
    else:
        conf_src_path = args[0]

    if os.path.isfile(conf_src_path) == 0:
        print("ERROR: Input source code file '{file}' is not a file."
              .format(file=conf_src_path))
        sys.exit(ERR_INVALID_ARG)

    if opt.built_in == 1:
        my_cnf = MY_CNF_DEFAULT + MY_CNF_DEFAULT_STORAGE
    elif opt.built_in == 0:
        my_cnf = MY_CNF_DEFAULT
    else:
        print("ERROR: Invalid built-in option. Please use 1 or 0")
        sys.exit(ERR_INVALID_ARG)

    if opt.project_type == 'mariadb':
        my_cnf = my_cnf + MARIADB_CNF_APPEND
    elif opt.project_type == 'mysql':
        my_cnf = my_cnf + MYSQL_CNF_APPEND
    else:
        print("ERROR: Invalid project-type option. Please use 'mysql' or"
              " 'mariadb'")
        sys.exit(ERR_INVALID_ARG)

    try:
        open(conf_src_path, 'r')
    except IOError as x:
        if x.errno == os.errno.ENOENT:
            print("ERROR: FILE '{file}' not exist".format(file=conf_src_path))
        elif x.errno == os.errno.EACCESS:
            print("ERROR: FILE '{file}' access permission denied"
                  .format(file=conf_src_path))
        else:
            print("ERROR: Open FILE {file}' failed".format(file=conf_src_path))
        sys.exit(ERR_INVALID_ARG)

    extractor = DocExtractor(language)
    tuples = extractor.get_tuples(conf_src_path)
    if not tuples:
        print("ERROR: Failed to parse document")
        sys.exit(ERR_PARSE)

    exporter = DocExporter(language, format)
    exporter.export(tuples, output_dir, my_cnf)
        
if __name__ == '__main__':
    sys.exit(main())


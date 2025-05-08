#!/usr/bin/env python3
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
import pdb
import stat

NAME = "build_thirdparty.py"
DESCRIPTION = "build_thirdparty - build the thirdparty projects."
USAGE = ""
EXTENDED_HELP = ""

current_dir=""
openssl_version = "1.1.1g"
curl_version="7.69.0"
curses_version="5.9"
jobs = 0

class BuildThirdParty(optparse.OptionParser):
    current_dir = ""
    debug=False
    jobs=0
    arch="x86_64"
    def print_help(self, output=None):
        optparse.OptionParser.print_help(self, output)

    def format_epilog(self, formatter):
        return self.epilog if self.epilog is not None else ''

    def set_debug(self, is_debug):
        self.debug=is_debug

    def set_arch(self, architect):
        self.arch=architect

    def set_jobs(self, job_num):
        self.jobs=job_num

    def build_openssl(self, version):
        #pdb.set_trace()
        openssl_dir = "openssl-"
        openssl_dir = openssl_dir + version
        debug_option=""
        openssl_abs_dir = self.current_dir + "/" + openssl_dir
        if not os.path.exists(openssl_abs_dir):
            print("The OpenSSL version:{v}, dir:'{d}' not exists."
            .format(v=version, d=openssl_abs_dir))
            return
        os.chdir(openssl_abs_dir)
        st = os.stat("./config")
        os.chmod("./config", st.st_mode | stat.S_IEXEC)
        if self.debug:
            debug_option=" -d"
        else:
            debug_option=""
        config_cmd = "./config --prefix=" + openssl_abs_dir + "/install_path" + \
                     " -fPIC" + debug_option
        print(config_cmd)
        os.system(config_cmd)
        make_cmd = "make -j " + str(self.jobs)
        make_install_cmd = "make install"
        rm_dynamic_libs = "rm -f install_path/lib/lib*.so; rm -f install_path/lib/lib*.so.*"
        os.system(make_cmd)
        os.system(make_install_cmd)
        os.system(rm_dynamic_libs)

    def build_curl(self, version):
        curl_dir = "curl-"
        curl_dir = curl_dir + version
        curl_abs_dir = self.current_dir + "/" +curl_dir
        debug_option=""
        if not os.path.exists(curl_abs_dir):
            print("The Curl version:{v}, dir:'{d}' not exists."
            .format(v=version, d=curl_abs_dir))
            return
        if self.debug:
            debug_option=" --enable-debug"
        else:
            debug_option=""
        os.chdir(curl_abs_dir)
        st = os.stat("./configure")
        os.chmod("./configure", st.st_mode | stat.S_IEXEC)
        config_cmd = "./configure --prefix=" + curl_abs_dir + "/install_path" + \
                     " --with-pic" + debug_option + " --without-ssl"
        os.system(config_cmd)
        make_cmd = "make -j " + str(self.jobs)
        make_install_cmd = "make install"
        rm_dynamic_libs = "rm -f install_path/lib/lib*.so; rm -f install_path/lib/lib*.so.*"
        os.system(make_cmd)
        os.system(make_install_cmd)
        os.system(rm_dynamic_libs)

    def build_unix_odbc(self, version):
        unix_odbc_dir = "unixODBC-"
        unix_odbc_dir = unix_odbc_dir + version
        unix_odbc_abs_dir = self.current_dir + "/" +unix_odbc_dir
        debug_option=""
        build=""
        host=""
        if not os.path.exists(unix_odbc_abs_dir):
            print("The UnixODBC version:{v}, dir:'{d}' not exists."
            .format(v=version, d=unix_odbc_abs_dir))
            return
        if self.debug:
            debug_option=" --enable-debug"
        else:
            debug_option=""

        if self.arch == "aarch64":
            build=" --build=arm-linux"
            host=" --host=arm-linux"
        os.chdir(unix_odbc_abs_dir)
        st = os.stat("./configure")
        os.chmod("./configure", st.st_mode | stat.S_IEXEC)
        config_cmd = "./configure --prefix=" + unix_odbc_abs_dir + "/install_path" + " --with-pic" +\
                     debug_option + " --enable-ltdl-install" + build + host
        os.system(config_cmd)
        make_cmd = "make -j " + str(self.jobs)
        make_install_cmd = "make install"
        os.system(make_cmd)
        os.system(make_install_cmd)

    def build_curses(self, version):
        curses_dir = "ncurses-"
        curses_dir = curses_dir + version
        curses_abs_dir = self.current_dir + "/" +curses_dir
        debug_option=""
        build=""
        if not os.path.exists(curses_abs_dir):
            print("The Curses version:{v}, dir:'{d}' not exists."
            .format(v=version, d=curses_abs_dir))
            return
        if self.debug:
            debug_option=" --enable-debug"
        else:
            debug_option=""
        if self.arch == "aarch64":
            build=" --build=arm-linux"
        os.chdir(curses_abs_dir)
        st = os.stat("./configure")
        os.chmod("./configure", st.st_mode | stat.S_IEXEC)
        config_cmd = "./configure --prefix=" + curses_abs_dir + "/install_path" + \
                     " CFLAGS=-fPIC CPPFLAGS=-fPIC" + debug_option + build
        os.system(config_cmd)
        make_cmd = "make -j " + str(self.jobs)
        make_install_cmd = "make install"
        rm_dynamic_libs = "rm -f install_path/lib/lib*.so; rm -f install_path/lib/lib*.so.*"
        os.system(make_cmd)
        os.system(make_install_cmd)
        os.system(rm_dynamic_libs)

def main():
    #pdb.set_trace()
    current_dir = os.path.dirname(os.path.realpath(__file__))

    # Setup the command parser
    program = os.path.basename(sys.argv[0]).replace(".py", "")
    builder = BuildThirdParty(
        description = DESCRIPTION,
        usage = USAGE,
        epilog = EXTENDED_HELP,
        prog = program
    )
    builder.current_dir = current_dir
    #Add --openssl option
    builder.add_option("-o", "--openssl", action='store', type="string",
                       dest="openssl", help="Build OpenSSL. Specify the "
                       "version of OpenSSL to be built, 1.1.1g.")
    #Add --curl option
    builder.add_option("-c", "--curl", action='store', type="string",
                       dest="curl", help="Build Curl. Specify the version "
                       "of Curl to be built, 7.69.0.")
    #Add --unix-odbc option
    builder.add_option("--odbc", action='store', type="string",
                       dest="odbc", help="Build unixODBC. Specify the "
                       "version of unixODBC to be built, eg: 2.3.1")

    #Add --curses option
    builder.add_option("--curses", action='store', type="string",
                       dest="curses", help="Build Curses. Specify the "
                       "version of Curses to be built, eg: 5.9")

    #Add --debug option
    builder.add_option("-d", "--debug", action='store_true', dest="debug",
                       help="Build debug, build release without '-d' defaultly.")

    #Add --jobs option
    builder.add_option("-j", "--jobs", action='store', type=int,
                       dest="jobs", help="Allow N jobs at once, "
                       "default: num of cpu count.")

    #Add --arch
    builder.add_option("-a", "--arch", action='store', type="string",
                       dest="arch", help="The target platform architect for building." )

    opt, args = builder.parse_args()
    
    if not opt.openssl and not opt.curl and not opt.odbc:
        builder.print_help()

    if opt.jobs:
        jobs = opt.jobs
    else:
        jobs = os.cpu_count()

    if opt.debug:
        builder.set_debug(True);

    if opt.arch == "aarch64":
        builder.set_arch("aarch64");

    builder.set_jobs(jobs);
    if opt.openssl:
        openssl_version = opt.openssl
        builder.build_openssl(openssl_version);

    if opt.curl:
        curl_version = opt.curl
        builder.build_curl(curl_version);

    if opt.odbc:
        unix_odbc_version = opt.odbc
        builder.build_unix_odbc(unix_odbc_version)

    if opt.curses:
        curses_version = opt.curses
        builder.build_curses(curses_version)

if __name__ == '__main__':
    sys.exit(main())


let $version_prefix = 'Version: ';
let $db_version = `select substring(COMMENT,locate($version_prefix,COMMENT)+length($version_prefix),1) from information_schema.ENGINES where engine = 'SequoiaDB'`;
if ($db_version < 5)
{
    --skip testcase, sequoiadb version is $db_version.
}

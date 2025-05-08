let $version_prefix = 'Version: ';
let $db_version = `select substring(COMMENT,locate($version_prefix,COMMENT)+length($version_prefix),3) from information_schema.ENGINES where engine = 'SequoiaDB'`;
if ( `select $db_version>=5 and $db_version < 5.6` )
{
    --skip testcase, sequoiadb version is $db_version.
}

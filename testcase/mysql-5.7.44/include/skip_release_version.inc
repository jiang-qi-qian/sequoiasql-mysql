let $version_prefix='Enterprise';
let $char_after_Enterprise = `select substring(COMMENT,locate($version_prefix,COMMENT)+length($version_prefix)+1,5) from information_schema.ENGINES where engine = 'SequoiaDB'`;
if ( $char_after_Enterprise!='Debug' )
{
    --skip testcase, package version is release.
}

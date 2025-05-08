let $sql_version = `select version() like '%debug%'`;
if ($sql_version == 0)
{
    --skip testcase, sequoiasql version is release.
}

# skip testcase if RC trans version is not within [5.0, 5.6)

let $version_prefix = 'Version: ';
# get sequoiadb version, e.g: 5.6.0
let $db_version = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(COMMENT, $version_prefix, -1), '(', 1) AS db_version FROM information_schema.ENGINES WHERE ENGINE = 'SequoiaDB'`;

# get major and mimor version, e.g: 5.6
let $substr_version = `SELECT SUBSTRING_INDEX('$db_version', '.', 2) AS first_number`;

# get major version, e.g: 5
let $major_version = `SELECT SUBSTRING_INDEX($substr_version, '.', 1) AS first_number`;

# get mimor version, e.g: 6
let $mimor_version = `SELECT SUBSTRING_INDEX($substr_version, '.', -1) AS second_number`;

if ( `select $major_version != 5 || ($major_version = 5 && $mimor_version >= 6)` )
{
   --skip testcase, the trans is RC.
}

#! /bin/bash
# Define log 

host="127.0.0.1"
port="3306"
inst_group=""


function build_help()
{
  cat <<EOF
      Usage: 
        sdb_fk_init.sh -u <user_name> -p <passwd>  -h <host> -P <port> [--grant-user=<user>] [--inst-group=<name>]
                             
      Options:
        -u , --user <Administrator>        User of administrator for login in MySQL.
        -p , --password <password>         Password of adminstrator user for login in MySQL. 
        -P , --port <port>                 Port number to use for connection to MySQL, default 3306
        -h , --host <localhost>            MySQL host address, default 127.0.0.1.
        --grant-user=<user>                Users who need to be granted privileges for the referential_constraints table,
                                           eg:--grant-user=user1[,user2,user3...]
        --inst-group=<name>                Instance group name.
EOF
}


function grant_to_customer()
{
  local custmor_array=()

  oldIFS=$IFS  
  IFS=,
  for item in ${customer_info} 
  do
    custmor_array+=($item)
  done
  IFS=$oldIFS
  
  local count=${#custmor_array[@]}

  for ((i=0;i<count;i++))
  do
    ${INSTALL_PATH}/bin/mysql -u${userName} "${pstr}" -h${host} -P${port} -e "
    GRANT ALL ON sequoiadb_foreign_config.* to '${custmor_array[$i]}'@'%';
    " 2>&1 | grep -v 'Warning'
    if [ ${PIPESTATUS[0]} = 0 ];then
      echo "Succeed to grant sequoiadb_foreign_config privileges to <${custmor_array[$i]}>."
    else
      echo "fail to grant sequoiadb_foreign_config privileges to <${custmor_array[$i]}>."
    fi
  done
}


function init_all(){
  

  ${INSTALL_PATH}/bin/mysql -u${userName} "${pstr}" -h${host} -P${port} -e " 

  CREATE database IF NOT EXISTS sequoiadb_foreign_config;
  use sequoiadb_foreign_config;
  SET session sequoiadb_support_mode='';
  CREATE table IF NOT EXISTS referential_constraints${inst_group}
  (
    foreign_key_name varchar(64) default NULL,
    trigger_name varchar(64)  default NULL,
    database_name varchar(64) NOT NULL,
    table_name varchar(64) NOT NULL,
    column_name varchar(1024) NOT NULL,
    referenced_database_name varchar(64) NOT NULL,
    referenced_table_name varchar(64) NOT NULL,
    referenced_column_name varchar(1024) NOT NULL,
    status enum('NULL','normal','deleted') COLLATE utf8mb4_general_ci default NULL ,
    md5 varchar(34) default NULL,
    INDEX(database_name,table_name),
    UNIQUE INDEX(foreign_key_name)
  )engine=sequoiadb COMMENT='sequoiadb:{auto_partition:false}';

  DROP trigger IF EXISTS sdb_trig_insert;
  delimiter $
  CREATE trigger sdb_trig_insert BEFORE INSERT ON referential_constraints
  FOR EACH ROW
  BEGIN
      declare return_rows int(11);
      declare msg varchar(1000);
      SELECT count(*) into return_rows from referential_constraints
      WHERE database_name=new.database_name and\
      table_name=new.table_name and\
      column_name=new.column_name and\
      referenced_database_name=new.referenced_database_name and\
      referenced_table_name=new.referenced_table_name and\
      referenced_column_name=new.referenced_column_name;

      IF return_rows <> 0 then
        set msg = 'The same foreign key constraint already exists';
        signal sqlstate '23000' set message_text = msg;
      END IF;
  END;
  $
  delimiter ;
  " 2>&1 | grep -v 'Warning'

if [ ${PIPESTATUS[0]} != 0 ];then
  echo "Can't create table sequoiadb_foreign_config.referential_constraints."
  exit
fi

echo "Succeed to create table sequoiadb_foreign_config.referential_constraints."

return
}

test $# -eq 0 && { build_help && exit 64; }


ARGS=`getopt -o p::u:h:P: -l user:,help,port:,host:,grant-user:,inst-group:,password: -n 'sdb_fk_init.sh' -- "$@"`
test $? -ne 0 && exit $?

#get path
dir_name=`dirname $0`
if [[ ${dir_name:0:1} != "/" ]]; then
   BIN_PATH=$(pwd)/$dir_name  #relative path
else
   BIN_PATH=$dir_name         #absolute path
fi

cur_path=`pwd`
cd $BIN_PATH/../../ && INSTALL_PATH=`pwd`
cd $cur_path >> /dev/null >&1

eval set -- "${ARGS}"

while true
do 
  case "$1" in
     -u |--user)             userName=$2
                             shift 2
                             ;;
     -p |--password)
      case $2 in
        "")                  # for `read` keep the spaces before and after the string
                             OLD_IFS="${IFS}"
                             IFS=''
                             read -s -p "Enter your password:" passWord
                             IFS="${OLD_IFS}"
                             echo ""
                             shift 2
                             ;;
        *)                   passWord=$2
                             shift 2
                             ;;
        esac 
        ;;
     -h |--host)             host=$2
                             shift 2
                             ;;
     -P |--port)             port=$2
                             shift 2
                             ;;
     --grant-user)           customer_info="$2"
                             shift 2
                             ;;
     --inst-group)           inst_group="_$2"
                             shift 2
                             ;;
     --help )                build_help
                             exit 0
                             ;;
     --)                     shift
                             break
                             ;;
     *)                      echo "error!"
                             exit 64
                             ;;
  esac
done

if [ "${passWord}" = "" ];then
  pstr=""
else
  pstr="-p${passWord}"
fi

init_all

if [ "${customer_info}" != "" ];then
  grant_to_customer
fi





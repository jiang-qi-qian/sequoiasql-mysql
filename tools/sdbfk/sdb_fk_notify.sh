#! /bin/bash

foreign_key_name=0
trigger_name=1
database_name=2
table_name=3
column_name=4
referenced_database_name=5
referenced_table_name=6
referenced_column_name=7
m_status=8
m_md5=9

oldIFS=$IFS

str_arr=()         #用来保存一条记录的所有字段中的values
userName=""        #保存登录的用户名
passWord=""        #保存登录的密码
host="127.0.0.1"   #默认host
port="3306"        #默认端口号
fk_name=""         #保存fk_name，保证能够单独处理某一条记录
inst_group=""

function build_help()
{
  cat <<EOF
      Options:
      -u , --user <username>            User for login in MySQL. 
      -p , --password <password>        Password for user when connect to MySQL.
      -P , --port <port>                Port number to use for connection to MySQL, default 3306.
      -h , --host <localhost>           MySQL host address, default 127.0.0.1.
      --fk_name=name                    Specify a foreigh key has changed the definition and need to refresh in MySQL.
      --inst-group=<name>               Instance group name.
EOF
}

#执行sql语句
function exec_sql()
{
  #$2有参数的情况下不打印错误到屏幕，目前只有添加索引时需要这种情况
  if [ "$2" = "" ];then
    ${INSTALL_PATH}/bin/mysql -u${userName} "${pstr}" -h${host} -P${port} -N -e " ${1} " 2>&1 | grep -v 'Warning'
  else
    ${INSTALL_PATH}/bin/mysql -u${userName} "${pstr}" -h${host} -P${port} -N -e " ${1} " > /dev/null 2>&1
  fi
}


#单独抽出来做添加fk_name的操作
function update_fk_name()
{ 
  #拼接字符串
  local fk_str="${str_arr[$table_name]}_sdbfk_${intercept_md5}"

  create_index_sql="
  UPDATE sequoiadb_foreign_config.referential_constraints${inst_group} set
    foreign_key_name='${fk_str}'
    where 
    database_name='${str_arr[$database_name]}' 
    and
    table_name='${str_arr[$table_name]}'
    and
    column_name='${str_arr[$column_name]}'
    and
    referenced_database_name='${str_arr[$referenced_database_name]}'
    and
    referenced_table_name='${str_arr[$referenced_table_name]}'
    and
    referenced_column_name='${str_arr[$referenced_column_name]}'
    ;               
  "
  exec_sql "${create_index_sql}" 
}


#添加触发器且更新trigger_name，md5的值,添加子表的索引和更新status
function add_status_and_trigger()
{
  local column_array=()
  local ref_column_array=()
  local condition=""

  #循环读取column_name里面的字段，因为有可能有多列做外键
  IFS=,
  for item in ${str_arr[$column_name]}
  do 
    column_array+=($item)
  done
  
  for item in ${str_arr[$referenced_column_name]}
  do
     ref_column_array+=($item)
  done
  IFS=$oldIFS

  #计算数组长度
  local count=${#column_array[@]}
  for ((i=0;i<count;i++))
    do
      condition+="\`${str_arr[$database_name]}\`."
      condition+="\`${str_arr[$table_name]}\`."
      condition+="\`${column_array[$i]}\`"
      condition+="=old.\`${ref_column_array[$i]}\`"
      if [ $i -ne $[$count-1] ]; then
        condition+=" and"
      fi
    done


  add_tri_sql="
  USE \`${str_arr[$referenced_database_name]}\`
  DROP trigger if exists \`${res_md5}\`;
  delimiter $
  CREATE trigger \`${res_md5}\` before delete on \`${str_arr[$referenced_table_name]}\` 
  for each row
  BEGIN
      declare result int(11);
      declare msg varchar(1000);
      declare return_rows int;

      select 1 into result from 
      \`${str_arr[$database_name]}\`.\`${str_arr[$table_name]}\` 
      where ${condition} limit 1;
      select found_rows() into return_rows;

      IF return_rows <> 0 then
        set msg = 'cannot delete a row in\
        ${str_arr[$referenced_database_name]}.${str_arr[$referenced_table_name]}:\ 
        a foreign key constraint on\ 
        ${str_arr[$database_name]}.${str_arr[$table_name]}';

        signal sqlstate '23000' set message_text = msg;
      END IF;
  END;
  $
  delimiter ;
  "

  create_index_sql="
  USE \`${str_arr[$database_name]}\`
  ALTER TABLE \`${str_arr[$table_name]}\` ADD INDEX(\`${str_arr[$column_name]}\`)
  "


  update_sql="
  UPDATE sequoiadb_foreign_config.referential_constraints${inst_group} SET 
    md5='${res_md5}', 
    trigger_name='${res_md5}',
    status='normal'
    where 
    database_name='${str_arr[$database_name]}' 
    and
    table_name='${str_arr[$table_name]}'
    and
    column_name='${str_arr[$column_name]}'
    and
    referenced_database_name='${str_arr[$referenced_database_name]}'
    and
    referenced_table_name='${str_arr[$referenced_table_name]}'
    and
    referenced_column_name='${str_arr[$referenced_column_name]}'
    ;
  "

 
  exec_sql "${add_tri_sql}"

  exec_sql "${create_index_sql}" "ignore"

  exec_sql "${update_sql}" 
  }


#当有列更新column后
function update_trigger()
{
  local rs=0
  add_status_and_trigger 
  drop_trigger
  return 
} 

function delete_record()
{
  delete_record_sql="    
    delete from sequoiadb_foreign_config.referential_constraints${inst_group} 
    where 
    trigger_name='${str_arr[$trigger_name]}';
    "
  exec_sql "${delete_record_sql}"
}


#删除对应的触发器
function drop_trigger()
{
  drop_sql="
    use \`${str_arr[$referenced_database_name]}\`
    drop trigger \`${str_arr[$trigger_name]}\`;
  "
  exec_sql "${drop_sql}"
  return 
}

#检查状态，根据状态的不同选择操作，是添加触发器、删除触发器还是更新配置表与触发器
function check_status()
{
  local val_str=""

  if [ "${str_arr[$database_name]}" = "" ] || \
     [ "${str_arr[$table_name]}" = "" ] || \
     [ "${str_arr[$column_name]}" = "" ] || \
     [ "${str_arr[$referenced_database_name]}" = "" ] || \
     [ "${str_arr[$referenced_table_name]}" = "" ] || \
     [ "${str_arr[$referenced_column_name]}" = "" ]   ;then
    #todo错误码确定下
     echo "The required items are not filled in correctly."
     exit 1
  fi

#做一个循环拼接字符串
  for((i=2;i<8;i++))
    do
      val_str=${val_str}${str_arr[$i]}
    done

#拼接md5的值
  local res_md5=$(echo -n "${val_str}"| md5sum |cut -d" " -f 1)
#截取md5值的前6位作为fk_name的规则之一
  local intercept_md5=${res_md5:0:6}
  if [ "${str_arr[$foreign_key_name]^^}" = "NULL" ] ; then
    update_fk_name
  fi

#通过status的值与算出来的值判断是否需要进行对应的操作
  if [ "${res_md5}" = "${str_arr[$m_md5]}" ] && \
     [ "${str_arr[$m_status]^^}" = "NORMAL" ] ; then
     return
  elif [ "${str_arr[$m_status]^^}" = "DELETED" ] ; then
    drop_trigger
    delete_record
    rs=$?
  elif  [ "${str_arr[$m_status]^^}" = "NULL" ] ; then
    add_status_and_trigger 
    rs=$?
  else
    update_trigger
    rs=$?
  fi

  return
}


test $# -eq 0 && { build_help && exit 64; }

ARGS=`getopt -o p::u:h:P: -l user:,help,port:,host:,password:,fk_name:,inst-group: -n 'sdb_fk_notify.sh' -- "$@"`
test $? -ne 0 && exit $?


#get path
dir_name=`dirname $0`
if [[ ${dir_name:0:1} != "/" ]]; then
   BIN_PATH=$(pwd)/$dir_name  #relative path
else
   BIN_PATH=$dir_name         #absolute path
fi

cur_path=`pwd`
cd $BIN_PATH/../.. && INSTALL_PATH=`pwd`
cd $cur_path >> /dev/null >&1

eval set -- "${ARGS}"

while true
  do
    case "$1" in
       -u |--user)               userName=$2
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
       -h |--host)               host=$2
                                 shift 2
                                 ;;
       -P |--port)               port=$2
                                 shift 2
                                 ;;
       --help )                  build_help
                                 exit 0
                                 ;;
       --fk_name )               fk_name=$2
                                 shift 2
                                 ;;
       --inst-group)             inst_group="_$2"
                                 shift 2
                                 ;;
       --)                       shift
                                 break
                                 ;;
       *)                        echo "error!"
                                 break
                                 ;;
    esac
  done


if test -z ${userName}; then
   echo "Please enter the user name"
   exit 
fi

if [ "${fk_name}" = "" ];then
  fk_condition=""
else
  fk_condition="where foreign_key_name='${fk_name}'"
fi

#脚本指定mysql的位置
if [ "${passWord}" = "" ]; then
  pstr=""
else
  pstr="-p${passWord}"
fi

${INSTALL_PATH}/bin/mysql -u${userName} "${pstr}" -h${host} -P${port} -N -e  "
select foreign_key_name,
      trigger_name,
      database_name,
      table_name,
      column_name,
      referenced_database_name,
      referenced_table_name,
      referenced_column_name,
      status,
      md5 from sequoiadb_foreign_config.referential_constraints${inst_group}
      ${fk_condition} \G
" 2>&1 | grep -v 'Warning' | while read line
do
  for((i=0 ; i<10 ;i++))
  do
    read line
    str_arr[$i]="${line}"
  done
  check_status "str_arr[$foreign_key_name]"\
               "str_arr[$trigger_name]"\
               "str_arr[$database_name]"\
               "str_arr[$table_name]"\
               "str_arr[$column_name]"\
               "str_arr[$referenced_database_name]"\
               "str_arr[$referenced_table_name]"\
               "str_arr[$referenced_column_name]"\
               "str_arr[$m_status]"\
               "str_arr[$m_md5]"
done
exit 

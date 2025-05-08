#!/bin/bash
######################################################################
#@decript:     judge that mysql can upgrade to new version or not
#@author:      CSQ 2019-06-28
#@input:       $1: old version
#              $2: new version
#              $3: --sql
#@return :     true | false
#@return code: 0:    normal
#              1:    format error
#              2/3:  unexpect error
#              64:   input error
######################################################################
function checkVsnPara()
{
   local vsn="$1"
   local vsnArr=(`tr "." " " <<< $vsn`)
   local len=${#vsnArr[@]}

   #can't be null
   if [ "X$vsn" == "X" ]
   then
      echo "error format of version : $vsn (null value)"
      exit 1
   fi

   #can't be more than 4, eg 3.4.8.1
   if [ $len -gt 4 ]
      then
         echo "error format of version : $vsn"
         exit 1
   fi

   #make sure number value
   for v in ${vsnArr[@]}
   do
      n=`echo "$v" | grep "[^0-9]" | wc -l`
      if [ $n -gt 0 ];
      then
         echo "error format of version : $vsn"
         exit 1
      fi
   done
}

# add zero to version, eg "1.12" -> "1.12.0.0"
function formatVsn()
{
   local vsn="$1"
   local vsnArr=(`tr "." " " <<< $vsn`)
   local len=${#vsnArr[@]}

   if [ $len -eq 1 ]; then vsn="${vsn}.0.0.0"; echo $vsn; return 0; fi
   if [ $len -eq 2 ]; then vsn="${vsn}.0.0";   echo $vsn; return 0; fi
   if [ $len -eq 3 ]; then vsn="${vsn}.0";     echo $vsn; return 0; fi

   echo $vsn; return 0;
}

#compare version1 to version2 ,if v1 < v2, echo "less"
function compareTwoVsn()
{
   local vsn1=$1
   local vsn2=$2
   local vsnArr1=(`tr "." " " <<< $vsn1`)
   local vsnArr2=(`tr "." " " <<< $vsn2`)

   local result="null"

   for(( i=0; i<$vsnLen; i++ ))
   do
      local v1=${vsnArr1[i]}
      local v2=${vsnArr2[i]}

      if [ $v1 -eq $v2 ]
      then
         result="equal"
         continue
      fi

      if [ $v1 -gt $v2 ]
      then
         result="greater"
         break
      else
         result="less"
         break
      fi
   done

   echo $result
}

function buildHelp()
{
   echo "Usage:"
   echo "  compatible.sh <OLD-VER> <NEW-VER> --sql"
}

#Parse command line parameters
test $# -eq 0 && exit 64

ARGS=`getopt -o h -l sql,help -n 'compatible.sh' -- "$@"`
ret=$?
test $ret -ne 0 && exit $ret

eval set -- "${ARGS}"

while true
do
   case "$1" in
      --sql )           mode="sql"
                       shift 1
                       ;;
      -h | --help )    buildHelp
                       shift 1
                       break
                       ;;
      --)              shift
                       break
                       ;;
      *)               exit 64
                       ;;
   esac
done

#################### main entry ############################
oldVsn=$1      
newVsn=$2     

checkVsnPara $oldVsn
checkVsnPara $newVsn

oldVsn=`formatVsn $oldVsn`
newVsn=`formatVsn $newVsn`

vsnLen=4
#compare with 3.2
result=`compareTwoVsn $oldVsn 3.2.0.0`
if [ $result == "less" ];   then echo false;  exit 3; fi

result=`compareTwoVsn $oldVsn $newVsn`
if [ $result == "equal" ];   then echo true;  exit 0; fi
if [ $result == "null" ];    then echo false; exit 2; fi
if [ $result == "less" ];    then echo true; exit 0; fi
if [ $result == "greater" ]; then echo false;  exit 0; fi

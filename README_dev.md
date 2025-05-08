# SequoiaSQL-MySQL Storage Engine

SequoiaSQL-MySQL Storage Engine is a distributed MySQL storage engine.

It currently supports SequoiaDB 3.x as the backend database, and it will be extended to multiple databases such like MongoDB/Redis and etc...

In order to take advantages of scalability and performance, SequoiaSQL-MySQL Storage Engine can be used to replace InnoDB and store user data/index/lob in the backend distributed database.

## Building.
1. Get the source code of SequoiaSQL-MySQL from gitlab.  
```bash
git clone http://gitlab.sequoiadb.com/sequoiadb/sequoiasql-mysql.git sequoiasql-mysql
```
2. Get the [SequoiaDB C++ driver 3.4](http://cdn.sequoiadb.com/images/sequoiadb-3.4/x86_64/driver/C%26CPP-3.4-linux_x86_64.tar.gz).
3. Configure the build.
```bash
cd sequoiasql-mysql
mkdir build
cd build
cmake .. -DMYSQL=5.7.25 -DWITH_SSL=</absolute/path/of/sequoiasql-mysql/thirdparty/openssl-1.0.1c> -DWITH_SDB_DRIVER=</path/to/sequoiadb/cpp/driver> -DCMAKE_INSTALL_PREFIX=</path/to/install/mysql/> -DCMAKE_BUILD_TYPE=Debug
```
SequoiaSQL-MySQL Storage Engine also supports the MySQL(5.7.24/5.7.28) and MariaDB(10.4.6). Build different version of MySQL or MariaDB w
ith argument `-DProjectType=version`. For example:
```bash
cd sequoiasql-mysql
mkdir build
cd build
cmake .. -DMARIADB=10.4.6 -DWITH_SSL=</absolute/path/of/sequoiasql-mysql/thirdparty/openssl-1.0.1c> -DWITH_SDB_DRIVER=</path/to/sequoiadb/cpp/driver> -DCMAKE_INSTALL_PREFIX=</path/to/install/mariadb/> -DCMAKE_BUILD_TYPE=Debug
```
4. Compile and install.
```bash
make -j 4
make install
```
> Note:  
> To simplify the compile, you can build with build script `build.py` instead of step 3~5.
> ```bash
> python3 build.py --sdbdriver </path/to/sequoiadb/cpp/driver> -t mysql-5.7.25 -i </path/to/install/mysql/> --clean --archivetest --dd
> ```
>or build mariadb-10.4.6
> ```bash
>python3 build.py --sdbdriver </path/to/sequoiadb/cpp/driver> -t mariadb-10.4.6 -i </path/to/install/mariadb/> --clean --archivetest --dd
> ```

## Testing the SequoiaSQL-MySQL server.
> Prerequisites:  
> SequoiaSQL-MySQL server can acess a SequoiaDB Cluster.   

SequoiaSQL-MySQL using the MySQL testing framework defined in `mysql-test` folder. To run all tests:
```
cd mysql-test
./mtr --suite=main,json --big-test --force --max-test-fail=0 --parallel=4
```
To run only one test:
```
cd mysql-test
./mtr --suite=<main_or_json> <test_case_name>
```

## Coding Guidelines

According to [MySQL coding guidelines](https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_CODING_GUIDELINES.html), we use the [Google C++ coding style](https://google.github.io/styleguide/cppguide.html).

We use [`clang-format`](http://clang.llvm.org/docs/ClangFormat.html) to format source code by 'Google' style with some exceptions. The '.clang-format' file contains all the options we used.

The `.clang-format` file is dumped from the tool by following command:
```bash
clang-format -style=google -dump-config > .clang-format
```

And we changed following options:
```
SortIncludes: false
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
```

You can directly use clang-format command line or [the plugin in VSCode](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format) if you use VSCode. Remember to use the `.clang-format` file as style.

## License

License under the GPL License, Version 2.0. See LICENSE for more information.
Copyright (c) 2018, SequoiaDB and/or its affiliates. All rights reserved.

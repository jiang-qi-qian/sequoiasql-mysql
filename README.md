# SequoiaSQL - MySQL Storage Engine

SequoiaSQL - MySQL Storage Engine is a distributed MySQL storage engine.

It currently supports SequoiaDB 3.x as the backend database, and it will be extended to multiple databases such like MongoDB/Redis and etc...

In order to take advantages of scalability and performance, SequoiaSQL - MySQL Storage Engine can be used to replace InnoDB and store user data/index/lob in the backend distributed database.


## Building

1. Get boost-1.59.0, the source code of mysql-5.7.24 and SequoiaDB C++ driver 3.0.1.
2. Clone the plugin code to the storage directory.
 ```bash
cd mysql-5.7.24/storage
git clone https://github.com/SequoiaDB/sequoiasql-mysql.git sequoiadb
 ```
3. Build the plugin
 ```bash
cd mysql-5.7.24
cmake . -DWITH_BOOST=</path/to/boost_1_59_0/> -DCMAKE_INSTALL_PREFIX=/opt/mysql -DMYSQL_DATADIR=/opt/mysql/data -DWITH_SDB_DRIVER=</path/to/sequoiadb/driver> -DCMAKE_BUILD_TYPE=Release
make install -j 4
 ```

By default, the sequoiadb storage engine is built into MySQL. You can add `-DSDB_BUILT_IN=OFF` to build it as a dynamic library.

Additional, we use two options to give plugin version and SequoiaDB C++ driver version:
```
-DSDB_PLUGIN_VERSION=<git commit short hash>
-DSDB_DRIVER_VERSION=<driver version>
```

For example: `-DSDB_PLUGIN_VERSION=4811624 -DSDB_DRIVER_VERSION=3.0.1`.

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

The MariaDB Audit Plugin is for logging the server's activity. For each  
client session, it records who connected to the server(i.e., user name and    
host), what queries were executed, and which tables were accessed and server  
variables that were changed.  

This directory of `lib` include the MariaDB Audit Plugin dynamic libraries   
are built for MySQL instance of SequoiaSQL-MySQL. The MySQL instance of  
SequoiaSQL-MySQL use MariaDB Audit Plugin for logging the server's activity.  
And the version of audit plugin MySQL instance of SequoiaSQL-MySQL used is   
1.4.5. Since the audit plugin is included in MariaDB. The latest version of  
MariaDB is 10.2.24 which includ 1.4.5 audit plugin. So the server_audit.so in  
this directory is built by MariaDB 10.2.24.

```shell
lib
+---aarch64 //built for arm architecture.
|   +---debug
|   |   +---server_audit.so
|   +---release
|       +---server_audit.so
+---x86_64 //built for x86_64 architecture.
    +---debug
    |   +---server_audit.so
    +---release
        +---server_audit.so
```

The MariaDB instance of SequoiaSQL-MySQL use its own audit plugin included in  
`lib/plugin`, which is built by its own version of MariaDB source code.


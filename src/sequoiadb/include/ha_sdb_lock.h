/* Copyright (c) 2019, SequoiaDB and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SDB_LOCK__H
#define SDB_LOCK__H
#include "ha_sdb_sql.h"

#if defined IS_MYSQL
#include <thr_mutex.h>
#include <thr_rwlock.h>
#elif defined IS_MARIADB
#include "my_pthread.h"
#endif

class Sdb_mutex {
  native_mutex_t m_mutex;

 public:
  Sdb_mutex() { native_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST); }

  ~Sdb_mutex() { native_mutex_destroy(&m_mutex); }

  inline int lock() { return native_mutex_lock(&m_mutex); }

  inline int unlock() { return native_mutex_unlock(&m_mutex); }
};

class Sdb_mutex_guard {
  Sdb_mutex &m_mutex;

 public:
  Sdb_mutex_guard(Sdb_mutex &mutex) : m_mutex(mutex) { m_mutex.lock(); }

  ~Sdb_mutex_guard() { m_mutex.unlock(); }
};

class Sdb_rwlock {
  native_rw_lock_t rw_lock;

 public:
  Sdb_rwlock() { native_rw_init(&rw_lock); }

  ~Sdb_rwlock() { native_rw_destroy(&rw_lock); }

  inline int read_lock() { return native_rw_rdlock(&rw_lock); }

  inline int write_lock() { return native_rw_wrlock(&rw_lock); }

  inline int unlock() { return native_rw_unlock(&rw_lock); }
};

class Sdb_rwlock_read_guard {
  Sdb_rwlock &m_rwlock;

 public:
  Sdb_rwlock_read_guard(Sdb_rwlock &lock) : m_rwlock(lock) {
    m_rwlock.read_lock();
  }

  ~Sdb_rwlock_read_guard() { m_rwlock.unlock(); }
};

class Sdb_rwlock_write_guard {
  Sdb_rwlock &m_rwlock;

 public:
  Sdb_rwlock_write_guard(Sdb_rwlock &lock) : m_rwlock(lock) {
    m_rwlock.write_lock();
  }

  ~Sdb_rwlock_write_guard() { m_rwlock.unlock(); }
};

#endif /* SDB_LOCK__H */

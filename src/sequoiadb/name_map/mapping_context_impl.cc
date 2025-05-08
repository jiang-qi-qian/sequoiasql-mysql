#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "ha_sdb_sql.h"
#include "mapping_context_impl.h"
#include "ha_sdb.h"
#include "ha_sdb_log.h"

const char *Mapping_context_impl::get_mapping_cs() {
  if (m_share && (*m_share) && '\0' != (*m_share)->mapping_cs[0] &&
      '\0' != (*m_share)->mapping_cl[0] && '\0' == m_cs_name[0]) {
    strncpy(m_cs_name, (*m_share)->mapping_cs, SDB_CS_NAME_MAX_SIZE);
  }
  return m_cs_name;
}

void Mapping_context_impl::set_mapping_cs(const char *cs_name) {
  strncpy(m_cs_name, cs_name, SDB_CS_NAME_MAX_SIZE);
  if (m_share && (*m_share)) {
    strncpy((*m_share)->mapping_cs, m_cs_name, SDB_CS_NAME_MAX_SIZE);
  }
}

const char *Mapping_context_impl::get_mapping_cl() {
  if (m_share && (*m_share) && '\0' != (*m_share)->mapping_cs[0] &&
      '\0' != (*m_share)->mapping_cl[0] && '\0' == m_cl_name[0]) {
    strncpy(m_cl_name, (*m_share)->mapping_cl, SDB_CL_NAME_MAX_SIZE);
  }
  return m_cl_name;
}

void Mapping_context_impl::set_mapping_cl(const char *cl_name) {
  strncpy(m_cl_name, cl_name, SDB_CL_NAME_MAX_SIZE);
  if (m_share && (*m_share)) {
    strncpy((*m_share)->mapping_cl, m_cl_name, SDB_CL_NAME_MAX_SIZE);
  }
}

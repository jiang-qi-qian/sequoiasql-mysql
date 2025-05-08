#ifndef MAPPING_CONTEXT_IMPL__H
#define MAPPING_CONTEXT_IMPL__H
#include "ha_sdb_def.h"
#include "mapping_context.h"
#include "ha_sdb_share.h"

class Mapping_context_impl : public Mapping_context {
 private:
  boost::shared_ptr<Sdb_share> *m_share;
  enum_mapping_state m_state;
  bool m_is_part_table;
  char m_cs_name[SDB_CS_NAME_MAX_SIZE + 1];
  char m_cl_name[SDB_CL_NAME_MAX_SIZE + 1];

 public:
  Mapping_context_impl()
      : m_share(NULL), m_state(NM_STATE_NONE), m_is_part_table(false) {
    m_cs_name[0] = '\0';
    m_cl_name[0] = '\0';
  }
  Mapping_context_impl(bool is_part_table)
      : m_share(NULL), m_state(NM_STATE_NONE), m_is_part_table(is_part_table) {
    m_cs_name[0] = '\0';
    m_cl_name[0] = '\0';
  }
  void set_share_cache(boost::shared_ptr<Sdb_share> *share) { m_share = share; }

  const char *get_mapping_cs();
  void set_mapping_cs(const char *cs_name);
  const char *get_mapping_cl();
  void set_mapping_cl(const char *cl_name);
  enum_mapping_state get_mapping_state() { return m_state; }
  void set_mapping_state(enum_mapping_state state) { m_state = state; }
  bool is_part_table() { return m_is_part_table; }
  void set_part_table(bool is_part_table) { m_is_part_table = is_part_table; }

  void reset() {
    m_share = NULL;
    m_state = NM_STATE_NONE;
    m_is_part_table = false;
    m_cs_name[0] = '\0';
    m_cl_name[0] = '\0';
  }
};
#endif

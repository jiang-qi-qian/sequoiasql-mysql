#ifndef MAPPING_CONTEXT__H
#define MAPPING_CONTEXT__H

enum enum_mapping_state {
  NM_STATE_NONE = 0,
  NM_STATE_CREATING,
  NM_STATE_CREATED
};

class Mapping_context {
 public:
  virtual ~Mapping_context() {}
  virtual const char *get_mapping_cs() = 0;
  virtual void set_mapping_cs(const char *cs_name) = 0;
  virtual const char *get_mapping_cl() = 0;
  virtual void set_mapping_cl(const char *cl_name) = 0;
  virtual enum_mapping_state get_mapping_state() = 0;
  virtual void set_mapping_state(enum_mapping_state state) = 0;
  virtual bool is_part_table() = 0;
  virtual void set_part_table(bool is_part_table) = 0;
};
#endif

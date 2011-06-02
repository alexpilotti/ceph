/**
 * All operations via the rados gateway are carried out by
 * small classes known as RGWOps. This class contains a req_state
 * and each possible command is a subclass of this with a defined
 * execute() method that does whatever the subclass name implies.
 * These subclasses must be further subclassed (by interface type)
 * to provide additional virtual methods such as send_response or get_params.
 */
#ifndef CEPH_RGW_OP_H
#define CEPH_RGW_OP_H

#include <string>

#include "rgw_common.h"
#include "rgw_access.h"
#include "rgw_user.h"
#include "rgw_acl.h"

using namespace std;

struct req_state;

/** Get the HTTP request metadata */
extern void get_request_metadata(struct req_state *s, map<string, bufferlist>& attrs);
/**
 * Get the ACL for an object off of disk. If you hold the req_state, use next
 * method.
 */
extern int read_acls(RGWAccessControlPolicy *policy, string& bucket, string& object);
/** Get the ACL needed for a request off of disk.*/
extern int read_acls(struct req_state *s, bool only_bucket = false);

/**
 * Provide the base class for all ops.
 */
class RGWOp {
protected:
  struct req_state *s;
public:
  RGWOp() {}
  virtual ~RGWOp() {}

  virtual void init(struct req_state *s) {
    this->s = s;
  }
  virtual void execute() = 0;
};

class RGWGetObj : public RGWOp {
protected:
  const char *range_str;
  const char *if_mod;
  const char *if_unmod;
  const char *if_match;
  const char *if_nomatch;
  off_t ofs;
  size_t len;
  size_t total_len;
  off_t end;
  time_t mod_time;
  time_t lastmod;
  time_t unmod_time;
  time_t *mod_ptr;
  time_t *unmod_ptr;
  map<string, bufferlist> attrs;
  char *data;
  int ret;
  bool get_data;

  int init_common();
public:
  RGWGetObj() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    range_str = NULL;
    if_mod = NULL;
    if_unmod = NULL;
    if_match = NULL;
    if_nomatch = NULL;
    ofs = 0;
    len = 0;
    total_len = 0;
    end = -1;
    mod_time = 0;
    lastmod = 0;
    unmod_time = 0;
    mod_ptr = NULL;
    unmod_ptr = NULL;
    attrs.clear();
    data = NULL;
    ret = 0;

    /* get_data should not be initialized here! */
  }
  void set_get_data(bool get_data) {
    this->get_data = get_data;
  }
  void execute();

  virtual int get_params() = 0;
  virtual int send_response(void *handle) = 0;
};

class RGWListBuckets : public RGWOp {
protected:
  int ret;
  RGWUserBuckets buckets;

public:
  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    buckets.clear();
  }
  RGWListBuckets() {}

  void execute();

  virtual void send_response() = 0;
};

class RGWListBucket : public RGWOp {
protected:
  string prefix;
  string marker; 
  string max_keys;
  string delimiter;
  int max;
  int ret;
  vector<RGWObjEnt> objs;
  map<string, bool> common_prefixes;

  string limit_opt_name;
  int default_max;

public:
  RGWListBucket() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    prefix.clear();
    marker.clear();
    max_keys.clear();
    delimiter.clear();
    max = 0;
    ret = 0;
    objs.clear();
    common_prefixes.clear();
  }
  void execute();

  virtual void send_response() = 0;
};

class RGWStatBucket : public RGWOp {
protected:
  int ret;
  RGWBucketEnt bucket;

public:
  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    bucket.clear();
  }
  RGWStatBucket() {}
  ~RGWStatBucket() {}

  void execute();

  virtual void send_response() = 0;
};

class RGWCreateBucket : public RGWOp {
protected:
  int ret;

public:
  RGWCreateBucket() {}

  void execute();
  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
  }
  virtual void send_response() = 0;
};

class RGWDeleteBucket : public RGWOp {
protected:
  int ret;

public:
  RGWDeleteBucket() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
  }
  void execute();

  virtual void send_response() = 0;
};

class RGWPutObj : public RGWOp {
protected:
  int ret;
  size_t len;
  off_t ofs;
  char *data;
  char *supplied_md5_b64;
  string etag;

public:
  RGWPutObj() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    len = 0;
    ofs = 0;
    data = NULL;
    supplied_md5_b64 = NULL;
    etag = "";
  }
  void execute();

  virtual int get_params() = 0;
  virtual int get_data() = 0;
  virtual void send_response() = 0;
};

class RGWDeleteObj : public RGWOp {
protected:
  int ret;

public:
  RGWDeleteObj() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
  }
  void execute();

  virtual void send_response() = 0;
};

class RGWCopyObj : public RGWOp {
protected:
  const char *if_mod;
  const char *if_unmod;
  const char *if_match;
  const char *if_nomatch;
  off_t ofs;
  off_t len;
  off_t end;
  time_t *mod_ptr;
  time_t *unmod_ptr;
  int ret;
  map<string, bufferlist> attrs;
  string src_bucket;
  string src_object;
  time_t mtime;

  int init_common();
public:
  RGWCopyObj() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    if_mod = NULL;
    if_unmod = NULL;
    if_match = NULL;
    if_nomatch = NULL;
    ofs = 0;
    len = 0;
    end = -1;
    mod_ptr = NULL;
    unmod_ptr = NULL;
    ret = 0;
    attrs.clear();
    src_bucket.clear();
    src_object.clear();
    mtime = 0;
  }
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
};

class RGWGetACLs : public RGWOp {
protected:
  int ret;
  string acls;

public:
  RGWGetACLs() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    acls.clear();
  }
  void execute();

  virtual void send_response() = 0;
};

class RGWPutACLs : public RGWOp {
protected:
  int ret;
  size_t len;
  char *data;

public:
  RGWPutACLs() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    len = 0;
    data = NULL;
  }
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
};

class RGWInitMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;

public:
  RGWInitMultipart() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    upload_id = "";
  }
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
};

class RGWCompleteMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;
  string etag;
  char *data;
  int len;

public:
  RGWCompleteMultipart() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    upload_id = "";
    etag="";
    data = NULL;
    len = 0;
  }
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
};

class RGWListMultipart : public RGWOp {
protected:
  int ret;
  string upload_id;
  map<uint32_t, RGWUploadPartInfo> parts;
  int max_parts;
  int marker;
  RGWAccessControlPolicy policy;

public:
  RGWListMultipart() {}

  virtual void init(struct req_state *s) {
    RGWOp::init(s);
    ret = 0;
    upload_id = "";
    parts.clear();
    max_parts = 1000;
    marker = 0;
    policy = RGWAccessControlPolicy();
  }
  void execute();

  virtual int get_params() = 0;
  virtual void send_response() = 0;
};

class RGWHandler {
protected:
  struct req_state *s;

  int do_read_permissions(bool only_bucket);
public:
  RGWHandler() {}
  virtual ~RGWHandler() {}
  static void init_state(struct req_state *s, struct fcgx_state *fcgx);

  void set_state(struct req_state *_s) { s = _s; }

  virtual RGWOp *get_op() = 0;
  virtual int read_permissions() = 0;
  virtual bool authorize(struct req_state *s) = 0;
};

#endif

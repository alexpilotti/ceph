// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph distributed storage system
 *
 * Copyright (C) 2013,2014 Cloudwatt <libre.licensing@cloudwatt.com>
 * Copyright (C) 2014 Red Hat <contact@redhat.com>
 *
 * Author: Loic Dachary <loic@dachary.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 * 
 */

#include "PluginRegistry.h"
#include "ceph_ver.h"
#include "common/ceph_context.h"
#include "common/errno.h"
#include "common/debug.h"
#include "common/shared_lib.h"

#define PLUGIN_PREFIX "libceph_"
#define PLUGIN_SUFFIX SHARED_LIB_SUFFIX
#define PLUGIN_INIT_FUNCTION "__ceph_plugin_init"
#define PLUGIN_VERSION_FUNCTION "__ceph_plugin_version"

#define dout_subsys ceph_subsys_context

PluginRegistry::PluginRegistry(CephContext *cct) :
  cct(cct),
  loading(false),
  disable_dlclose(false)
{
}

PluginRegistry::~PluginRegistry()
{
  if (disable_dlclose)
    return;

  for (std::map<std::string,std::map<std::string, Plugin*> >::iterator i =
	 plugins.begin();
       i != plugins.end();
       ++i) {
    for (std::map<std::string,Plugin*>::iterator j = i->second.begin();
	 j != i->second.end(); ++j) {
      shared_lib_handle_t library = j->second->library;
      delete j->second;
      shared_lib_close(library);
    }
  }
}

int PluginRegistry::remove(const std::string& type, const std::string& name)
{
  ceph_assert(ceph_mutex_is_locked(lock));

  std::map<std::string,std::map<std::string,Plugin*> >::iterator i =
    plugins.find(type);
  if (i == plugins.end())
    return -ENOENT;
  std::map<std::string,Plugin*>::iterator j = i->second.find(name);
  if (j == i->second.end())
    return -ENOENT;

  ldout(cct, 1) << __func__ << " " << type << " " << name << dendl;
  shared_lib_handle_t library = j->second->library;
  delete j->second;
  shared_lib_close(library);
  i->second.erase(j);
  if (i->second.empty())
    plugins.erase(i);

  return 0;
}

int PluginRegistry::add(const std::string& type,
			const std::string& name,
			Plugin* plugin)
{
  ceph_assert(ceph_mutex_is_locked(lock));
  if (plugins.count(type) &&
      plugins[type].count(name)) {
    return -EEXIST;
  }
  ldout(cct, 1) << __func__ << " " << type << " " << name
		<< " " << plugin << dendl;
  plugins[type][name] = plugin;
  return 0;
}

Plugin *PluginRegistry::get_with_load(const std::string& type,
          const std::string& name)
{
  std::lock_guard l(lock);
  Plugin* ret = get(type, name);
  if (!ret) {
    int err = load(type, name);
    if (err == 0)
      ret = get(type, name);
  } 
  return ret;
}

Plugin *PluginRegistry::get(const std::string& type,
			    const std::string& name)
{
  ceph_assert(ceph_mutex_is_locked(lock));
  Plugin *ret = 0;

  std::map<std::string,Plugin*>::iterator j;
  std::map<std::string,map<std::string,Plugin*> >::iterator i =
    plugins.find(type);
  if (i == plugins.end()) 
    goto out;
  j = i->second.find(name);
  if (j == i->second.end()) 
    goto out;
  ret = j->second;

 out:
  ldout(cct, 1) << __func__ << " " << type << " " << name
		<< " = " << ret << dendl;
  return ret;
}

int PluginRegistry::load(const std::string &type,
			 const std::string &name)
{
  ceph_assert(ceph_mutex_is_locked(lock));
  ldout(cct, 1) << __func__ << " " << type << " " << name << dendl;

  // std::string fname = cct->_conf->plugin_dir + "/" + type + "/" PLUGIN_PREFIX
  //  + name + PLUGIN_SUFFIX;
  std::string fname = cct->_conf.get_val<std::string>("plugin_dir") + "/" + type + "/" + PLUGIN_PREFIX
      + name + PLUGIN_SUFFIX;
  shared_lib_handle_t library = shared_lib_open(fname.c_str());
  if (!library) {
    char* err1 = shared_lib_last_err();
    // fall back to plugin_dir
    std::string fname2 = cct->_conf.get_val<std::string>("plugin_dir") + "/" + PLUGIN_PREFIX +
      name + PLUGIN_SUFFIX;
    library = shared_lib_open(fname2.c_str());
    if (!library) {
      char* err2 = shared_lib_last_err();
      lderr(cct) << __func__
		 << " failed dlopen(): \""	<< err1
		 << "\" or \"" << err2 << "\""
		 << dendl;
     shared_lib_free_err_msg(err2);
    }

    shared_lib_free_err_msg(err1);
    if (!library) {
      return -EIO;
    }
  }

  const char * (*code_version)() =
    (const char *(*)())shared_lib_find_symbol(library, PLUGIN_VERSION_FUNCTION);
  if (code_version == NULL) {
    char* err = shared_lib_last_err();
    lderr(cct) << __func__ << " code_version == NULL" << err << dendl;
    shared_lib_free_err_msg(err);
    return -EXDEV;
  }
  if (code_version() != string(CEPH_GIT_NICE_VER)) {
    lderr(cct) << __func__ << " plugin " << fname << " version "
	       << code_version() << " != expected "
	       << CEPH_GIT_NICE_VER << dendl;
    shared_lib_close(library);
    return -EXDEV;
  }

  int (*code_init)(CephContext *,
		   const std::string& type,
		   const std::string& name) =
    (int (*)(CephContext *,
	     const std::string& type,
	     const std::string& name))shared_lib_find_symbol(library, PLUGIN_INIT_FUNCTION);
  if (code_init) {
    int r = code_init(cct, type, name);
    if (r != 0) {
      lderr(cct) << __func__ << " " << fname << " "
		 << PLUGIN_INIT_FUNCTION << "(" << cct
		 << "," << type << "," << name << "): " << cpp_strerror(r)
		 << dendl;
      shared_lib_close(library);
      return r;
    }
  } else {
    char* err = shared_lib_last_err();
    lderr(cct) << __func__ << " " << fname << " dlsym(" << PLUGIN_INIT_FUNCTION
	       << "): " << err << dendl;
    shared_lib_free_err_msg(err);
    shared_lib_close(library);
    return -ENOENT;
  }

  Plugin *plugin = get(type, name);
  if (plugin == 0) {
    lderr(cct) << __func__ << " " << fname << " "
	       << PLUGIN_INIT_FUNCTION << "()"
	       << "did not register plugin type " << type << " name " << name
	       << dendl;
    shared_lib_close(library);
    return -EBADF;
  }

  plugin->library = library;

  ldout(cct, 1) << __func__ << ": " << type << " " << name
		<< " loaded and registered" << dendl;
  return 0;
}

/*
int ErasureCodePluginRegistry::preload(const std::string &plugins,
				       const std::string &directory,
				       ostream &ss)
{
  std::lock_guard l(lock);
  list<string> plugins_list;
  get_str_list(plugins, plugins_list);
  for (list<string>::iterator i = plugins_list.begin();
       i != plugins_list.end();
       ++i) {
    ErasureCodePlugin *plugin;
    int r = load(*i, directory, &plugin, ss);
    if (r)
      return r;
  }
  return 0;
}
*/

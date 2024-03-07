#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "modifiers.h"
#include "tables.h"
#include <json.h>
#include <json_object.h>

static const EGLint context_attribs[] = {
    EGL_CONTEXT_MAJOR_VERSION,
    2,
    EGL_NONE,
};

/*
static const EGLint config_attribs[] = {
    EGL_RED_SIZE,
    1,
    EGL_GREEN_SIZE,
    1,
    EGL_BLUE_SIZE,
    1,
    EGL_ALPHA_SIZE,
    1,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES_BIT,
    EGL_NONE,
};
*/

struct json_object *egl_dev_info(EGLDeviceEXT dev);

struct json_object *egl_info(char *paths[]) {
  const char *device_paths[16] = {0};
  EGLDeviceEXT devices[16] = {0};
  EGLint num_devices = 16;
  const PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT =
      (void *)eglGetProcAddress("eglQueryDevicesEXT");
  const PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT =
      (void *)eglGetProcAddress("eglQueryDeviceStringEXT");
  eglQueryDevicesEXT(num_devices, devices, &num_devices);
  for (int i = 0; i < num_devices; i++) {
    device_paths[i] =
        eglQueryDeviceStringEXT(devices[i], EGL_DRM_DEVICE_FILE_EXT);
    if (device_paths[i] == 0) {
      device_paths[i] = "";
    }
  }

  struct json_object *obj = json_object_new_object();
  for (int i = 0; i < num_devices; i++) {
    if (!paths[0]) {
      // Collect from all devices.
      struct json_object *dev = egl_dev_info(devices[i]);
      json_object_object_add(obj, device_paths[i], dev);
    } else {
      // Check if it was passed in.
      for (char **path = paths; *path; ++path) {
        if (!strcmp(*path, device_paths[i])) {
          struct json_object *dev = egl_dev_info(devices[i]);
          if (!dev)
            continue;

          json_object_object_add(obj, *path, dev);
        }
      }
    }
  }
  return obj;
}

struct json_object *egl_dev_info(EGLDeviceEXT dev) {
  const PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT =
      (void *)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
  const PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT =
      (void *)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
  // initialize EGL for wayland.
  int egl_major = 0;
  int egl_minor = 0;
  EGLDisplay display =
      eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, dev, NULL);
  assert(display);
  assert(eglInitialize(display, &egl_major, &egl_minor) == EGL_TRUE);
  assert(egl_major == 1);
  assert(egl_minor >= 5);
  EGLConfig configs[256];
  int num_configs = 256;
  int num_exists_configs = 0;
  eglBindAPI(EGL_OPENGL_ES_API);
  eglGetConfigs(display, configs, num_configs, &num_exists_configs);
  // Shits broke yo.
  // eglChooseConfig(display, config_attribs, configs, num_configs,
  // &num_configs); assert(num_configs > 0);
  assert(num_exists_configs > 0);
  EGLContext context =
      eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attribs);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

  struct json_object *obj = json_object_new_object();

  json_object_object_add(
      obj, "vendor",
      json_object_new_string(eglQueryString(display, EGL_VENDOR)));
  json_object_object_add(
      obj, "version",
      json_object_new_string(eglQueryString(display, EGL_VERSION)));
  json_object_object_add(
      obj, "renderer",
      json_object_new_string((const char *)glGetString(GL_RENDERER)));

  EGLint formats[256] = {0};
  EGLint num_formats = 256;
  if (eglQueryDmaBufFormatsEXT(display, num_formats, formats, &num_formats) !=
      EGL_TRUE) {
    // No dmabufs supported.
    return obj;
  }
  struct json_object *formats_arr = json_object_new_array();

  for (int f = 0; f < num_formats; f++) {
    EGLuint64KHR modifiers[256] = {0};
    EGLint num_modifiers = 256;
    assert(eglQueryDmaBufModifiersEXT(display, formats[f], num_modifiers,
                                      modifiers, NULL,
                                      &num_modifiers) == EGL_TRUE);
    struct json_object *format_obj = json_object_new_object();
    struct json_object *modifier_arr = json_object_new_array();
    for (int m = 0; m < num_modifiers; m++) {
      json_object_array_add(modifier_arr, json_object_new_uint64(modifiers[m]));
    }
    json_object_object_add(format_obj, "format",
                           json_object_new_uint64(formats[f]));
    json_object_object_add(format_obj, "modifiers", modifier_arr);
    json_object_array_add(formats_arr, format_obj);
  }

  json_object_object_add(obj, "formats", formats_arr);
  return obj;
}

static const char *get_object_object_string(struct json_object *obj,
                                            const char *key) {
  struct json_object *str_obj = json_object_object_get(obj, key);
  if (!str_obj) {
    return NULL;
  }
  return json_object_get_string(str_obj);
}

static uint64_t get_object_object_uint64(struct json_object *obj,
                                         const char *key) {
  struct json_object *uint64_obj = json_object_object_get(obj, key);
  if (!uint64_obj) {
    return 0;
  }
  return json_object_get_uint64(uint64_obj);
}

void print_egl(struct json_object *obj) {

  json_object_object_foreach(obj, path, device_obj) {
    (void)path;
    printf("vendor: %s\n", get_object_object_string(device_obj, "vendor"));
    printf("version: %s\n", get_object_object_string(device_obj, "version"));
    printf("renderer: %s\n", get_object_object_string(device_obj, "renderer"));

    struct json_object *formats_arr =
        json_object_object_get(device_obj, "formats");
    for (size_t i = 0; i < json_object_array_length(formats_arr); i++) {
      struct json_object *format_obj =
          json_object_array_get_idx(formats_arr, i);
      printf("format: %s\n",
             format_str(get_object_object_uint64(format_obj, "format")));
      struct json_object *modifiers_arr =
          json_object_object_get(format_obj, "modifiers");
      for (size_t j = 0; j < json_object_array_length(modifiers_arr); j++) {
        struct json_object *modifier_obj =
            json_object_array_get_idx(modifiers_arr, j);
        printf("modifier: ");
        print_modifier(json_object_get_uint64(modifier_obj));
        printf("\n");
      }
    }
  }
}

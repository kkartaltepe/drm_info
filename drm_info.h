#ifndef DRM_INFO_H
#define DRM_INFO_H

struct json_object;

struct json_object *egl_info(char *paths[]);
struct json_object *drm_info(char *paths[]);
void print_drm(struct json_object *obj);
void print_egl(struct json_object *obj);

/* according to CTA 861.G */
enum {
	HDMI_STATIC_METADATA_TYPE1 = 0
};

enum {
	CTA_EOTF_TRADITIONAL_SDR = 0,
	CTA_EOTF_TRADITIONAL_HDR,
	CTA_EOTF_SMPTE_2084,
	CTA_EOTF_HLG
};

#endif

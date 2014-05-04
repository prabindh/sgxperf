#if 0

#include <xf86drm.h>

#include <xf86drmMode.h>
#include <gbm.h>


static struct {
        struct gbm_device *dev;
        struct gbm_surface *surface;
} gbm;

static struct {
        int fd;
        uint32_t ndisp;
        uint32_t crtc_id[MAX_DISPLAYS];
        uint32_t connector_id[MAX_DISPLAYS];
        uint32_t resource_id;
        uint32_t encoder[MAX_DISPLAYS];
        drmModeModeInfo *mode[MAX_DISPLAYS];
        drmModeConnector *connectors[MAX_DISPLAYS];
} drm;

struct drm_fb {
        struct gbm_bo *bo;
        uint32_t fb_id;
};

static int init_drm(void)
{
        static const char *modules[] = {
                        "omapdrm", "i915", "radeon", "nouveau", "vmwgfx", "exynos"
        };
        drmModeRes *resources;
        drmModeConnector *connector = NULL;
        drmModeEncoder *encoder = NULL;
        int i, j;
        uint32_t maxRes, curRes;

        for (i = 0; i < ARRAY_SIZE(modules); i++) {
                printf("trying to load module %s...", modules[i]);
                drm.fd = drmOpen(modules[i], NULL);
                if (drm.fd < 0) {
                        printf("failed.\n");
                } else {
                        printf("success.\n");
                        break;
                }
        }

        if (drm.fd < 0) {
                printf("could not open drm device\n");
                return -1;
        }

        resources = drmModeGetResources(drm.fd);
        if (!resources) {
                printf("drmModeGetResources failed: %s\n", strerror(errno));
                return -1;
        }
        drm.resource_id = (uint32_t) resources;

        /* find a connected connector: */
        for (i = 0; i < resources->count_connectors; i++) {
                connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
                if (connector->connection == DRM_MODE_CONNECTED) {
                        /* choose the first supported mode */
                        drm.mode[drm.ndisp] = &connector->modes[0];
                        drm.connector_id[drm.ndisp] = connector->connector_id;

                        for (j=0; j<resources->count_encoders; j++) {
                                encoder = drmModeGetEncoder(drm.fd, resources->encoders[j]);
                                if (encoder->encoder_id == connector->encoder_id)
                                        break;

                                drmModeFreeEncoder(encoder);
                                encoder = NULL;
                        }

                        if (!encoder) {
                                printf("no encoder!\n");
                                return -1;
                        }

                        drm.encoder[drm.ndisp]  = (uint32_t) encoder;
                        drm.crtc_id[drm.ndisp] = encoder->crtc_id;
                        drm.connectors[drm.ndisp] = connector;

                        printf("### Display [%d]: CRTC = %d, Connector = %d\n", drm.ndisp, drm.crtc_id[drm.ndisp], drm.connector_id[drm.ndisp]);
                        printf("\tMode chosen [%s] : Clock => %d, Vertical refresh => %d, Type => %d\n", drm.mode[drm.ndisp]->name, drm.mode[drm.ndisp]->clock, drm.mode[drm.ndisp]->vrefresh, drm.mode[drm.ndisp]->type);
                        printf("\tHorizontal => %d, %d, %d, %d, %d\n", drm.mode[drm.ndisp]->hdisplay, drm.mode[drm.ndisp]->hsync_start, drm.mode[drm.ndisp]->hsync_end, drm.mode[drm.ndisp]->htotal, drm.mode[drm.ndisp]->hskew);
                        printf("\tVertical => %d, %d, %d, %d, %d\n", drm.mode[drm.ndisp]->vdisplay, drm.mode[drm.ndisp]->vsync_start, drm.mode[drm.ndisp]->vsync_end, drm.mode[drm.ndisp]->vtotal, drm.mode[drm.ndisp]->vscan);

                        /* If a connector_id is specified, use the corresponding display */
                        if ((connector_id != -1) && (connector_id == (int)drm.connector_id[drm.ndisp]))
                                DISP_ID = drm.ndisp;

                        /* If all displays are enabled, choose the connector with maximum
                        * resolution as the primary display */
                        if (all_display) {
                                maxRes = drm.mode[DISP_ID]->vdisplay * drm.mode[DISP_ID]->hdisplay;
                                curRes = drm.mode[drm.ndisp]->vdisplay * drm.mode[drm.ndisp]->hdisplay;

                                if (curRes > maxRes)
                                        DISP_ID = drm.ndisp;
                        }

                        drm.ndisp++;
                } else {
                        drmModeFreeConnector(connector);
                }
        }

        if (drm.ndisp == 0) {
                /* we could be fancy and listen for hotplug events and wait for
                 * a connector..
                 */
                printf("no connected connector!\n");
                return -1;
        }

        return 0;
}

static int init_gbm(void)
{
        gbm.dev = gbm_create_device(drm.fd);

        gbm.surface = gbm_surface_create(gbm.dev,
                        drm.mode[DISP_ID]->hdisplay, drm.mode[DISP_ID]->vdisplay,
                        GBM_FORMAT_XRGB8888,
                        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        if (!gbm.surface) {
                printf("failed to create gbm surface\n");
                return -1;
        }

        return 0;
}


static void exit_gbm(void)
{
        gbm_surface_destroy(gbm.surface);
        gbm_device_destroy(gbm.dev);
        return;
}

static void exit_egl(void)
{
        eglDestroySurface(gl.display, gl.surface);
        eglDestroyContext(gl.display, gl.context);
        eglTerminate(gl.display);
        return;
}

static void exit_drm(void)
{

        drmModeRes *resources;
        int i;

        resources = (drmModeRes *)drm.resource_id;
        for (i = 0; i < resources->count_connectors; i++) {
                drmModeFreeEncoder((drmModeEncoderPtr)drm.encoder[i]);
                drmModeFreeConnector(drm.connectors[i]);
        }
        drmModeFreeResources((drmModeResPtr)drm.resource_id);
        drmClose(drm.fd);
        return;
}
static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
        struct drm_fb *fb = (drm_fb*)data;

        if (fb->fb_id)
                drmModeRmFB(drm.fd, fb->fb_id);

        free(fb);
}

static struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
{
        struct drm_fb *fb = (drm_fb*)gbm_bo_get_user_data(bo);
        uint32_t width, height, stride, handle;
        int ret;

        if (fb)
                return fb;

        fb = (drm_fb*)calloc(1, sizeof *fb);
        fb->bo = bo;

        width = gbm_bo_get_width(bo);
        height = gbm_bo_get_height(bo);
        stride = gbm_bo_get_stride(bo);
        handle = gbm_bo_get_handle(bo).u32;

        ret = drmModeAddFB(drm.fd, width, height, 24, 32, stride, handle, &fb->fb_id);
        if (ret) {
                printf("failed to create fb: %s\n", strerror(errno));
                free(fb);
                return NULL;
        }

        gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

        return fb;
}

static void page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{
        int *waiting_for_flip = (int*)data;
        *waiting_for_flip = 0;
}

int init_egl_drm()
{
        fd_set fds;
        drmEventContext evctx = {
                        .version = DRM_EVENT_CONTEXT_VERSION,
                        .vblank_handler = 0,
                        .page_flip_handler = page_flip_handler,
        };
        struct gbm_bo *bo;
        struct drm_fb *fb;
        ret = init_drm();
        if (ret) {
                printf("failed to initialize DRM\n");
                return ret;
        }
        printf("### Primary display => ConnectorId = %d, Resolution = %dx%d\n",
                        drm.connector_id[DISP_ID], drm.mode[DISP_ID]->hdisplay,
                        drm.mode[DISP_ID]->vdisplay);

        FD_ZERO(&fds);
        FD_SET(drm.fd, &fds);

        ret = init_gbm();
        if (ret) {
                printf("failed to initialize GBM\n");
                return ret;
        }

	//Initialise egl regularly here
}

void egl_drm_draw_flip()
{
        /* set mode: */
        if (all_display) {
                for (i=0; i<drm.ndisp; i++) {
                        ret = drmModeSetCrtc(drm.fd, drm.crtc_id[i], fb->fb_id, 0, 0,
                                        &drm.connector_id[i], 1, drm.mode[i]);
                        if (ret) {
                                printf("display %d failed to set mode: %s\n", i, strerror(errno));
                                return ret;
                        }
                }
        } else {
                ret = drmModeSetCrtc(drm.fd, drm.crtc_id[DISP_ID], fb->fb_id,
                                0, 0, &drm.connector_id[DISP_ID], 1, drm.mode[DISP_ID]);
                if (ret) {
                        printf("display %d failed to set mode: %s\n", DISP_ID, strerror(errno));
                        return ret;
                }
        }
	//Draw call here
	//Swapping is involved
                next_bo = gbm_surface_lock_front_buffer(gbm.surface);
                fb = drm_fb_get_from_bo(next_bo);

                /*
                 * Here you could also update drm plane layers if you want
                 * hw composition
                 */

                ret = drmModePageFlip(drm.fd, drm.crtc_id[DISP_ID], fb->fb_id,
                                DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
                if (ret) {
                        printf("failed to queue page flip: %s\n", strerror(errno));
                        return -1;
                }

                while (waiting_for_flip) {
                        ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
                        if (ret < 0) {
                                printf("select err: %s\n", strerror(errno));
                                return ret;
                        } else if (ret == 0) {
                                printf("select timeout!\n");
                                return -1;
                        } else if (FD_ISSET(0, &fds)) {
                                continue;
                        }
                        drmHandleEvent(drm.fd, &evctx);
                }
                /* release last buffer to render on again: */
                gbm_surface_release_buffer(gbm.surface, bo);
                bo = next_bo;
}

#endif //untested as of now

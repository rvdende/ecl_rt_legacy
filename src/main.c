#include "ecl.h"
#include "image.h"
#include "v3.h"
#include <assert.h>
#include <stdio.h>

struct camera {
    v3 origin;
    v3 x, y, z;
    v3 viewport_center;
    f32 viewport_w, viewport_h;
};

struct camera* camera_init(struct camera *cam, v3 lookFrom, v3 lookAt, f32 aspect) {
    assert(aspect > 1.0f); // width > height only, please

    if (cam) {
        cam->origin = v3_sub(lookFrom, lookAt);
        cam->z = v3_normalize(cam->origin); // z axis points from origin to the camera; we look down -z axis
        cam->x = v3_normalize(v3_cross((v3){0, 0, 1}, cam->z));
        cam->y = v3_normalize(v3_cross(cam->z, cam->x));

        // position our viewport 'plate' 1 unit in front of the camera
        cam->viewport_center = v3_sub(cam->origin, v3_mulf(cam->z, 1.0f));
        cam->viewport_h = 1.0f;
        cam->viewport_w = cam->viewport_h * aspect;
    }

    return cam;
}

struct material {
    v3 color;
};

struct plane {
    v3 n;
    f32 d;
    u32 material;
};

struct sphere {
    v3 p;
    f32 r;
    u32 material;
};

struct world {
    u32 material_count;
    struct material *materials;
    u32 plane_count;
    struct plane *planes;
    u32 sphere_count;
    struct sphere *spheres;
};

static v3 cast(const struct world *w, v3 origin, v3 dir) {
    v3 result = w->materials[0].color; // default background color
    f32 hit_dist = F32_MAX;

    f32 tolerance = 0.0001f;

    for (u32 plane_idx = 0; plane_idx < w->plane_count; ++plane_idx) {
        struct plane *p = &w->planes[plane_idx];

        f32 denominator = v3_dot(p->n, dir);
        if (denominator < -tolerance || denominator > tolerance) {
            f32 t = (-p->d - v3_dot(p->n, origin)) / denominator;

            if (t > 0 && t < hit_dist) {
                result = w->materials[p->material].color;
                hit_dist = t;
            }
        }
    }

    for (u32 sphere_idx = 0; sphere_idx < w->sphere_count; ++sphere_idx) {
        struct sphere *s = &w->spheres[sphere_idx];

        v3 sphere_relative_origin = v3_sub(origin, s->p);
        f32 a = v3_dot(dir, dir);
        f32 b = 2.0f * v3_dot(dir, sphere_relative_origin);
        f32 c = v3_dot(sphere_relative_origin, sphere_relative_origin) - s->r;

        f32 denominator = 2.0f * a;
        f32 root_term = ecl_sqrt(b * b - 4.0f * a * c);
        if (root_term > tolerance) {
            f32 tp = (-b + root_term) / denominator;
            f32 tn = (-b - root_term) / denominator;

            f32 t = tp;
            if (tn > 0 && tn < tp) {
                t = tn;
            }

            if (t > 0 && t < hit_dist) {
                result = w->materials[s->material].color;
                hit_dist = t;
            }

        }
    }

    return result;
}

int main() {

    struct material materials[] = {
            {0.1f, 0.1f, 0.1f},
            {1,    0,    0},
            {0,    1,    0},
            {0,    0,    1},
    };

    struct plane p = {
            .n = {0, 0, 1},
            .d = 0,
            .material = 1,
    };

    struct sphere s[] = {
            {
                    .p = {0, 0, 1},
                    .r = 1.0f,
                    .material = 2,
            },
            {
                    .p = {3, -3, 0},
                    .r = 3.0f,
                    .material = 3,
            }
    };

    struct world w = {
            .material_count = 2,
            .materials = materials,
            .plane_count = 1,
            .planes = &p,
            .sphere_count = 2,
            .spheres = s,
    };

    struct image *img = image_new(1280, 720);

    struct camera cam;
    camera_init(&cam, (v3){0, -10, 1}, (v3){0, 0, 0}, (f32)img->width / img->height);

    u32 *pos = img->pixels;
    for (u32 y = 0; y < img->height; ++y) {
        f32 viewport_y = -1.0f + 2.0f * ((f32)y / (f32)img->height);
        for (u32 x = 0; x < img->width; ++x) {
            f32 viewport_x = -1.0f + 2.0f * ((f32)x / (f32)img->width);
            v3 movealongx = v3_mulf(cam.x, viewport_x * 0.5f * cam.viewport_w);
            v3 movealongy = v3_mulf(cam.y, viewport_y * 0.5f * cam.viewport_h);
            v3 viewpoint_p = v3_add(cam.viewport_center, movealongx);
            viewpoint_p = v3_add(viewpoint_p, movealongy);

            v3 ray_p = cam.origin;
            v3 ray_dir = v3_normalize(v3_sub(viewpoint_p, cam.origin));

            v3 color = cast(&w, ray_p, ray_dir);

            u32 bmp_pixel = (((u32)(255) << 24) |
                                  ((u32)(255.0f * color.r + 0.05f) << 16) |
                                  ((u32)(255.0f * color.g + 0.05f) << 8) |
                                  ((u32)(255.0f * color.b + 0.05f) << 0));
            *pos++ = bmp_pixel;
        }
    }
    write_image(img, "out.bmp");

    printf("Fin.\n");
    return 0;
}

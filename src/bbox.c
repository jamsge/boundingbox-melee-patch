#include "events.h"

// Draws a screen-space axis-aligned bounding box around every fighter.
//
// The box is the union of all of the fighter's hurtbox capsules. Each frame we
// build the world-space AABB, project its corners through the match camera to
// screen space, and draw the resulting rectangle.
//
// The rectangle is drawn through a dedicated overlay camera (orthographic,
// mapping normalized device coordinates straight to the screen) that renders
// after the world and the HUD, with depth testing disabled. This makes the
// boxes true screen-space rectangles that sit on top of everything.

// GX link rendered only by our overlay camera (must not clash with other links)
#define BBOX_GXLINK_OVERLAY 19
// camera object kind used by training mode's HUD camera
#define BBOX_CAM_OBJKIND R13_U8(-0x3E55)

#define BBOX_LINE_WIDTH 24
#define BBOX_PADDING 3.0f
#define BBOX_FALLBACK_HALF 15.0f

typedef struct BBox
{
    bool any;
    Vec3 min;
    Vec3 max;
} BBox;

typedef struct BBoxScreen
{
    float x0;
    float y0;
    float x1;
    float y1;
} BBoxScreen;

static GXColor stc_bbox_colors[4] = {
    {255, 0, 255, 255}, // P1 magenta
    {0, 255, 255, 255}, // P2 cyan
    {255, 255, 0, 255}, // P3 yellow
    {0, 255, 0, 255},   // P4 green
};

static COBJ *stc_overlay_cobj;

static void BBox_Expand(BBox *box, Vec3 p)
{
    if (!box->any)
    {
        box->min = p;
        box->max = p;
        box->any = true;
        return;
    }

    if (p.X < box->min.X) box->min.X = p.X;
    if (p.Y < box->min.Y) box->min.Y = p.Y;
    if (p.Z < box->min.Z) box->min.Z = p.Z;
    if (p.X > box->max.X) box->max.X = p.X;
    if (p.Y > box->max.Y) box->max.Y = p.Y;
    if (p.Z > box->max.Z) box->max.Z = p.Z;
}

static void BBox_ExpandRadius(BBox *box, Vec3 p, float radius)
{
    BBox_Expand(box, (Vec3) {p.X - radius, p.Y - radius, p.Z - radius});
    BBox_Expand(box, (Vec3) {p.X + radius, p.Y + radius, p.Z + radius});
}

// Compute the world-space AABB of a fighter from its hurtbox capsules.
static bool BBox_ComputeWorld(FighterData *ft_data, BBox *out)
{
    BBox box = {0};

    // We transform the capsule offsets through the bone matrix ourselves
    // instead of reading hurt1_pos/hurt2_pos: those cached values are only
    // refreshed during hitbox collision checks and are often stale.
    for (int i = 0; i < ft_data->hurt_num; i++)
    {
        FtHurt *hurt = &ft_data->hurtbox[i];

        // prefer the bone array (always valid); hurt->jobj can be junk during
        // some animations (e.g. Fox pulling out his blaster)
        JOBJ *bone = 0;
        if (hurt->bone_index >= 0 && hurt->bone_index < ft_data->bone_num)
            bone = ft_data->bones[hurt->bone_index].joint;
        if (bone == 0)
            bone = hurt->jobj;
        if (bone == 0)
            continue;

        Vec3 a;
        Vec3 b;
        JOBJ_GetWorldPosition(bone, &hurt->hurt1_offset, &a);
        JOBJ_GetWorldPosition(bone, &hurt->hurt2_offset, &b);

        BBox_ExpandRadius(&box, a, hurt->scale);
        BBox_ExpandRadius(&box, b, hurt->scale);
    }

    // fall back to the fighter origin if there are no hurtboxes (entry, etc.)
    if (!box.any)
    {
        Vec3 pos = ft_data->phys.pos;
        box.min = (Vec3) {pos.X - BBOX_FALLBACK_HALF, pos.Y, pos.Z};
        box.max = (Vec3) {pos.X + BBOX_FALLBACK_HALF, pos.Y + BBOX_FALLBACK_HALF * 2.f, pos.Z};
        box.any = true;
    }

    // padding
    box.min.X -= BBOX_PADDING;
    box.min.Y -= BBOX_PADDING;
    box.max.X += BBOX_PADDING;
    box.max.Y += BBOX_PADDING;

    *out = box;
    return true;
}

// Project a world point to normalized device coordinates of the given camera.
static bool BBox_ProjectNDC(COBJ *camera, Vec3 world, Vec3 *ndc)
{
    Vec3 screen;
    if (HSD_GXProject(camera, &world, &screen, 0) == 0)
        return false;

    float w = camera->viewport_right - camera->viewport_left;
    float h = camera->viewport_bottom - camera->viewport_top;
    if (w == 0.f || h == 0.f)
        return false;

    ndc->X = (screen.X - camera->viewport_left) / w * 2.f - 1.f;
    ndc->Y = (screen.Y - camera->viewport_top) / h * 2.f - 1.f;
    ndc->Z = screen.Z;
    return true;
}

// Transform a point from the overlay camera's view space to world space. The
// view matrix is a rigid transform, so its inverse is the transposed rotation
// applied to the translated point.
static Vec3 BBox_ViewToWorld(Mtx m, Vec3 view)
{
    float dx = view.X - m[0][3];
    float dy = view.Y - m[1][3];
    float dz = view.Z - m[2][3];

    Vec3 out;
    out.X = m[0][0] * dx + m[1][0] * dy + m[2][0] * dz;
    out.Y = m[0][1] * dx + m[1][1] * dy + m[2][1] * dz;
    out.Z = m[0][2] * dx + m[1][2] * dy + m[2][2] * dz;
    return out;
}

// Same setup as GFX_Start, but with depth testing disabled so the box always
// renders on top.
static void BBox_StartOverlay(u16 vtx_count, GFX_Params params)
{
    Mtx mtx;
    HSD_ClearVtxDesc();
    GXSetCurrentMtx(0);
    COBJ_GetViewingMtx(COBJ_GetCurrent(), &mtx);
    GXLoadPosMtxImm(mtx, 0);
    HSD_StateSetLineWidth(params.size, 5);
    HSD_StateSetPointSize(params.size, 5);
    HSD_SetupRenderMode(0x68000002);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    HSD_StateSetCullMode(0);
    GXSetZMode(GX_DISABLE, GX_LEQUAL, GX_DISABLE);
    GXBegin(params.shape, GX_VTXFMT0, vtx_count);
}

void BBox_GX(GOBJ *gobj, int pass)
{
    if (pass != 2 || stc_overlay_cobj == 0)
        return;

    // Match_GetCObj() actually returns the camera GOBJ; the COBJ is its object
    GOBJ *match_gobj = (GOBJ *)Match_GetCObj();
    COBJ *match = match_gobj != 0 ? (COBJ *)match_gobj->hsd_object : 0;
    if (match == 0)
        return;

    BBox boxes[4];
    int count = 0;

    for (int port = 0; port < 4; port++)
    {
        GOBJ *fighter_gobj = Fighter_GetGObj(port);
        if (fighter_gobj == 0)
            continue;

        FighterData *ft_data = fighter_gobj->userdata;
        if (ft_data == 0)
            continue;

        if (!BBox_ComputeWorld(ft_data, &boxes[count]))
            continue;

        count++;
    }

    if (count == 0)
        return;

    BBoxScreen rects[4];
    GXColor colors[4];
    int rect_count = 0;

    for (int i = 0; i < count; i++)
    {
        Vec3 corners[8] = {
            {boxes[i].min.X, boxes[i].min.Y, boxes[i].min.Z},
            {boxes[i].max.X, boxes[i].min.Y, boxes[i].min.Z},
            {boxes[i].max.X, boxes[i].max.Y, boxes[i].min.Z},
            {boxes[i].min.X, boxes[i].max.Y, boxes[i].min.Z},
            {boxes[i].min.X, boxes[i].min.Y, boxes[i].max.Z},
            {boxes[i].max.X, boxes[i].min.Y, boxes[i].max.Z},
            {boxes[i].max.X, boxes[i].max.Y, boxes[i].max.Z},
            {boxes[i].min.X, boxes[i].max.Y, boxes[i].max.Z},
        };

        float x0 = 1e30f, y0 = 1e30f;
        float x1 = -1e30f, y1 = -1e30f;
        int projected = 0;

        for (int c = 0; c < 8; c++)
        {
            Vec3 ndc;
            if (!BBox_ProjectNDC(match, corners[c], &ndc))
                continue;

            // skip non-finite projections (corners behind the camera, etc.)
            if (ndc.X != ndc.X || ndc.Y != ndc.Y)
                continue;
            if (ndc.X < -1e6f || ndc.X > 1e6f || ndc.Y < -1e6f || ndc.Y > 1e6f)
                continue;

            if (ndc.X < x0) x0 = ndc.X;
            if (ndc.Y < y0) y0 = ndc.Y;
            if (ndc.X > x1) x1 = ndc.X;
            if (ndc.Y > y1) y1 = ndc.Y;
            projected++;
        }

        if (projected == 0 || x0 > x1 || y0 > y1)
            continue;

        // clamp to the screen
        if (x0 < -1.f) x0 = -1.f;
        if (y0 < -1.f) y0 = -1.f;
        if (x1 > 1.f) x1 = 1.f;
        if (y1 > 1.f) y1 = 1.f;

        rects[rect_count] = (BBoxScreen) {x0, y0, x1, y1};
        colors[rect_count] = stc_bbox_colors[i];
        rect_count++;
    }

    if (rect_count == 0)
        return;

    BBox_StartOverlay(rect_count * 8, (GFX_Params) { .shape = GX_LINES, .size = BBOX_LINE_WIDTH });

    Mtx view;
    COBJ_GetViewingMtx(stc_overlay_cobj, &view);

    for (int i = 0; i < rect_count; i++)
    {
        BBoxScreen r = rects[i];

        Vec3 c[4] = {
            BBox_ViewToWorld(view, (Vec3) {r.x0, -r.y0, 0.f}),
            BBox_ViewToWorld(view, (Vec3) {r.x1, -r.y0, 0.f}),
            BBox_ViewToWorld(view, (Vec3) {r.x1, -r.y1, 0.f}),
            BBox_ViewToWorld(view, (Vec3) {r.x0, -r.y1, 0.f}),
        };

        GXColor color = colors[i];
        for (int e = 0; e < 4; e++)
        {
            GFX_AddVtx(c[e].X, c[e].Y, c[e].Z, color);
            GFX_AddVtx(c[(e + 1) % 4].X, c[(e + 1) % 4].Y, c[(e + 1) % 4].Z, color);
        }
    }
}

void BBox_CamThink(GOBJ *gobj)
{
    CObjThink_Common(gobj);
}

void BBox_Init(void)
{
    // Overlay camera: an orthographic camera whose x/y view coordinates map
    // directly to normalized device coordinates, so we can draw screen-space
    // rectangles exactly.
    GOBJ *cam_gobj = GObj_Create(19, 20, 0);
    COBJDesc ***dmgScnMdls = Archive_GetPublicAddress(*stc_ifall_archive, (void *)0x803f94d0);
    COBJ *cobj = COBJ_LoadDesc(dmgScnMdls[1][0]);
    CObj_SetOrtho(cobj, 1.f, -1.f, -1.f, 1.f);
    cobj->near = -1.f;
    cobj->far = 1.f;
    GObj_AddObject(cam_gobj, BBOX_CAM_OBJKIND, cobj);
    // Render priority: cameras are rendered in ascending priority order, and
    // the in-game HUD camera uses 8, so pick something higher to draw last.
    GOBJ_InitCamera(cam_gobj, BBox_CamThink, 50);
    cam_gobj->cobj_links = 1 << BBOX_GXLINK_OVERLAY;
    stc_overlay_cobj = cobj;

    // GXLink callback that draws the boxes while the overlay camera renders.
    GOBJ *draw_gobj = GObj_Create(0, 0, 0);
    GObj_AddGXLink(draw_gobj, BBox_GX, BBOX_GXLINK_OVERLAY, 0);
}

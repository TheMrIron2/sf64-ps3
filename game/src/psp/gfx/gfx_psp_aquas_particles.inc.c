static const Gfx* __attribute__((noinline)) psp_gfx_dl_aquas_particles(
    PspGfxDlContext* ctx, const Gfx* first, u32 depth) {
    const Gfx* next = first;
    Gfx restore[7];
    u32 count = 0;
    int drawn;
    psp_gfx_dl_pool_drain(ctx, PSP_PROFILE_FLUSH_RENDER_STATE_CHANGE);
    while (count < PSP_AQUAS_PARTICLE_BATCH &&
           ctx->stats.commandCount + (count + 1) * 2 < PSP_GFX_DL_MAX_COMMANDS &&
           next->words.w0 == ((u32) G_NOOP << 24) && next->words.w1 == PSP_AQUAS_PARTICLE_MARKER &&
           PspAquasParticles_ValidMatrix(next[1].words.w0) && !(next[1].words.w1 & ~0x3ffu)) {
        count++;
        next += 2;
    }
    if (!count) return first + 2;
    drawn = PspGfxBackend_IsGu() && ctx->hasProjection && PspAquasParticles_Draw(first, count, &ctx->projection[0][0], ctx->projectionSerial,
                                  psp_gfx_dl_resolve_viewport(ctx, NULL, 0, &ctx->projection[0][0]));
    if (!drawn) {
        u32 i;
        for (i = 0; i < count; i++) {
            PspAquasParticles_Restore(restore, first + i * 2, 1);
            psp_gfx_dl_run_internal(ctx, restore, depth + 1);
        }
    } else {
        PspAquasParticles_Restore(restore, next - 2, 0);
        psp_gfx_dl_run_internal(ctx, restore, depth + 1);
    }
    ctx->stats.commandCount += count * 2;
    return next;
}

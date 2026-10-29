/* Browser-only edition diagnostics; no firmware or sound mutation. */
EXPORT uint32_t web_additive_edition(void)
{
    return ENGINES[ENGI_ADD] == &ENG_ADD && eng_gone(ENGI_SAMPLE);
}
EXPORT uint32_t web_selected_engine(void) { return trk[song.sel].eng_req; }

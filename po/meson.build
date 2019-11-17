i18n = import('i18n')
add_project_arguments('-DGETTEXT_PACKAGE="wavbreaker"', language:'c')
i18n.gettext(meson.project_name(),
    args: '--directory=' + meson.source_root()
)

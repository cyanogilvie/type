pandoc = find_program('pandoc', native: true, required: false)

if pandoc.found()
  doc_md = configure_file(
    input:          'objtype.md.in',
    output:         '@BASENAME@',
    configuration:  teaconf,
  )

  doc_targets = []

  doc_targets += custom_target('manpage',
    input:    doc_md,
    output:   'objtype.n',
    command:  [pandoc, '--standalone', '--from', 'markdown', '--to', 'man',
      '@INPUT@', '--output', '@OUTPUT@'],
    install: true,
    install_dir:      get_option('mandir'),
    build_by_default: true,
  )

  alias_target('doc', doc_targets)
endif

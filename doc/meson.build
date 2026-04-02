pandoc = find_program('pandoc', native: true, required: false)

if pandoc.found()
  doc_md = configure_file(
    input:          'type.md.in',
    output:         '@BASENAME@',
    configuration:  teaconf,
  )

  doc_targets = []

  doc_targets += custom_target('htmldoc',
    input:    doc_md,
    output:   'type.html',
    command:  [pandoc, '--standalone', '--metadata', 'title=type',
      '--from', 'markdown', '--to', 'html',
      '@INPUT@', '--output', '@OUTPUT@'],
    install:          true,
    install_dir:      get_option('datadir') / 'doc',
    build_by_default: true,
  )
  doc_targets += custom_target('manpage',
    input:    doc_md,
    output:   'type.n',
    command:  [pandoc, '--standalone', '--from', 'markdown', '--to', 'man',
      '@INPUT@', '--output', '@OUTPUT@'],
    install:          true,
    install_dir:      get_option('mandir'),
    build_by_default: true,
  )

  alias_target('doc', doc_targets)
endif

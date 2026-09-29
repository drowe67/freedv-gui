#
# Translations for FreeDV.
#
# Translatable strings in the source are marked with _() (or wxTRANSLATE(),
# wxPLURAL() etc.). The following targets are available:
#
#   update-pot: regenerates po/freedv.pot from the current source code.
#   update-po:  merges po/freedv.pot into each language listed in po/LINGUAS.
#
# Compiled catalogs (.mo) for each language in po/LINGUAS are built as part
# of the normal build and are placed in ${CMAKE_BINARY_DIR}/share/locale so
# that FreeDV can find them when run directly from the build directory.
#

find_package(Gettext QUIET)
find_program(XGETTEXT_EXECUTABLE xgettext)

set(FREEDV_POT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/freedv.pot)
set(FREEDV_LOCALE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/share/locale)

# Languages to build, one per line. Lines starting with # are ignored.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS FREEDV_LINGUAS REGEX "^[^#]")
list(TRANSFORM FREEDV_LINGUAS STRIP)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS)

#
# Target to regenerate the translation template.
#
if(XGETTEXT_EXECUTABLE)
    file(GLOB_RECURSE FREEDV_TRANSLATABLE_SOURCES
        RELATIVE ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/src/*.mm)
    # Exclude third party code.
    list(FILTER FREEDV_TRANSLATABLE_SOURCES EXCLUDE REGEX "^src/(util/websocketpp|sox|pipeline/kiss_|pipeline/_kiss)")
    list(SORT FREEDV_TRANSLATABLE_SOURCES)

    add_custom_target(update-pot
        COMMAND ${XGETTEXT_EXECUTABLE}
            --from-code=UTF-8
            --language=C++
            --keyword=_
            --keyword=wxTRANSLATE
            --keyword=wxPLURAL:1,2
            --keyword=wxGETTEXT_IN_CONTEXT:1c,2
            --keyword=wxGETTEXT_IN_CONTEXT_PLURAL:1c,2,3
            --keyword=wxTRANSLATE_IN_CONTEXT:1c,2
            --add-comments=TRANSLATORS:
            --add-location=file
            --sort-by-file
            --package-name=FreeDV
            --package-version=${FreeDV_VERSION}
            --msgid-bugs-address=https://github.com/drowe67/freedv-gui/issues
            --output=${FREEDV_POT_FILE}
            ${FREEDV_TRANSLATABLE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Regenerating ${FREEDV_POT_FILE}"
        VERBATIM)
else(XGETTEXT_EXECUTABLE)
    message(STATUS "xgettext not found, update-pot target will not be available.")
endif(XGETTEXT_EXECUTABLE)

if(NOT GETTEXT_FOUND)
    if(FREEDV_LINGUAS)
        message(WARNING "gettext (msgfmt/msgmerge) not found -- FreeDV will be built without translations.")
    endif(FREEDV_LINGUAS)
    return()
endif(NOT GETTEXT_FOUND)

#
# Target to merge the template into existing translations.
#
add_custom_target(update-po)

#
# Compile each translation.
#
set(FREEDV_MO_FILES)
foreach(lang ${FREEDV_LINGUAS})
    set(po_file ${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po)
    set(mo_dir ${FREEDV_LOCALE_OUTPUT_DIR}/${lang}/LC_MESSAGES)
    set(mo_file ${mo_dir}/freedv.mo)

    add_custom_command(
        OUTPUT ${mo_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check-format -o ${mo_file} ${po_file}
        DEPENDS ${po_file}
        COMMENT "Compiling ${lang} translation"
        VERBATIM)
    list(APPEND FREEDV_MO_FILES ${mo_file})

    add_custom_command(
        TARGET update-po
        POST_BUILD
        COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none ${po_file} ${FREEDV_POT_FILE}
        VERBATIM)

    # When we build our own wxWidgets, also ship its translations for the
    # standard dialogs/buttons, as there's no system copy to fall back on.
    if(BOOTSTRAP_WXWIDGETS AND EXISTS ${wxwidgets_SOURCE_DIR}/locale/${lang}.po)
        set(wx_mo_file ${mo_dir}/wxstd.mo)
        add_custom_command(
            OUTPUT ${wx_mo_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${wx_mo_file} ${wxwidgets_SOURCE_DIR}/locale/${lang}.po
            DEPENDS ${wxwidgets_SOURCE_DIR}/locale/${lang}.po
            COMMENT "Compiling wxWidgets ${lang} translation"
            VERBATIM)
        list(APPEND FREEDV_MO_FILES ${wx_mo_file})
    endif()

    # On macOS the catalogs are copied into the app bundle instead (see src/CMakeLists.txt).
    if(NOT APPLE)
        install(
            DIRECTORY ${mo_dir}
            DESTINATION share/locale/${lang}
            FILES_MATCHING PATTERN "*.mo")
    endif(NOT APPLE)
endforeach(lang)

add_custom_target(freedv_translations ALL DEPENDS ${FREEDV_MO_FILES})

# Let the parent scope (and src/) know what was built.
set(FREEDV_LINGUAS ${FREEDV_LINGUAS} PARENT_SCOPE)
set(FREEDV_MO_FILES ${FREEDV_MO_FILES} PARENT_SCOPE)

add_test(NAME wkhtmltopdf-ng-version COMMAND wkhtmltopdf-ng --version)
add_test(NAME wkhtmltopdf-ng-help COMMAND wkhtmltopdf-ng --help)
add_test(NAME wkhtmltopdf-ng-cli-flags
    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/cli_flags.sh
        $<TARGET_FILE:wkhtmltopdf-ng>)
set_tests_properties(wkhtmltopdf-ng-version wkhtmltopdf-ng-help wkhtmltopdf-ng-cli-flags
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(UNIX)
    add_executable(test_capi test_capi.c)
    target_link_libraries(test_capi PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME wkhtmltox-capi-symbols
        COMMAND test_capi ${CMAKE_BINARY_DIR}/libwkhtmltox.so)
    set_tests_properties(wkhtmltox-capi-symbols
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(EXISTS "/usr/lib/qt6/libexec/QtWebEngineProcess")
    add_test(NAME wkhtmltox-capi-convert
        COMMAND test_capi
            ${CMAKE_BINARY_DIR}/libwkhtmltox.so
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(wkhtmltox-capi-convert
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)

    add_test(NAME wkhtmltopdf-ng-toc
        COMMAND wkhtmltopdf-ng --toc --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/toc.pdf)
    add_test(NAME wkhtmltopdf-ng-multi-input
        COMMAND wkhtmltopdf-ng --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/multi-second.html
            ${CMAKE_CURRENT_BINARY_DIR}/multi.pdf)
    add_test(NAME wkhtmltopdf-ng-resource-isolation
        COMMAND wkhtmltopdf-ng --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/resources.html
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/multi-second.html
            ${CMAKE_CURRENT_BINARY_DIR}/resources.pdf)
    set_tests_properties(wkhtmltopdf-ng-toc
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)
    set_tests_properties(wkhtmltopdf-ng-multi-input wkhtmltopdf-ng-resource-isolation
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)

    add_test(NAME wkhtmltopdf-ng-regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/regression.sh
            ${CMAKE_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/regression)
    set_tests_properties(wkhtmltopdf-ng-regression
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 120)

    add_test(NAME wkhtmltopdf-ng-viewport
        COMMAND wkhtmltopdf-ng --viewport-size 800x600 --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern.html
            ${CMAKE_CURRENT_BINARY_DIR}/viewport.pdf)
    add_test(NAME wkhtmltopdf-ng-no-background
        COMMAND wkhtmltopdf-ng --no-background --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern.html
            ${CMAKE_CURRENT_BINARY_DIR}/no-background.pdf)
    add_test(NAME wkhtmltopdf-ng-min-font
        COMMAND wkhtmltopdf-ng --minimum-font-size 18 --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern.html
            ${CMAKE_CURRENT_BINARY_DIR}/min-font.pdf)
    add_test(NAME wkhtmltopdf-ng-copies
        COMMAND wkhtmltopdf-ng --copies 2 --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/copies.pdf)
    add_test(NAME wkhtmltopdf-ng-no-compression
        COMMAND wkhtmltopdf-ng --no-pdf-compression --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/uncompressed.pdf)
    add_test(NAME wkhtmltopdf-ng-compress
        COMMAND wkhtmltopdf-ng --compress --compress-level 9 --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/compressed.pdf)
    add_test(NAME wkhtmltopdf-ng-cover-toc
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            cover ${CMAKE_CURRENT_SOURCE_DIR}/test_data/cover.html
            toc
            page ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/cover-toc.pdf)
    add_test(NAME wkhtmltopdf-ng-per-object-header
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/cover.html --header-center COVER
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html --header-center BODY
            ${CMAKE_CURRENT_BINARY_DIR}/per-object.pdf)
    add_test(NAME wkhtmltopdf-ng-load-skip
        COMMAND wkhtmltopdf-ng --load-error-handling skip --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/missing-does-not-exist.html
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
            ${CMAKE_CURRENT_BINARY_DIR}/load-skip.pdf)
    set_tests_properties(wkhtmltopdf-ng-viewport wkhtmltopdf-ng-no-background
        wkhtmltopdf-ng-min-font wkhtmltopdf-ng-copies wkhtmltopdf-ng-no-compression
        wkhtmltopdf-ng-compress
        wkhtmltopdf-ng-cover-toc wkhtmltopdf-ng-per-object-header
        wkhtmltopdf-ng-load-skip
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)

    add_test(NAME wkhtmltopdf-ng-page-ranges
        COMMAND wkhtmltopdf-ng --page-ranges 1 --javascript-delay 0
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/tall.html
            ${CMAKE_CURRENT_BINARY_DIR}/page-ranges.pdf)
    set_tests_properties(wkhtmltopdf-ng-page-ranges
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)

    add_test(NAME wkhtmltopdf-ng-dpi
        COMMAND wkhtmltopdf-ng --disable-smart-shrinking --dpi 192
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/tall.html
            ${CMAKE_CURRENT_BINARY_DIR}/dpi.pdf)
    add_test(NAME wkhtmltopdf-ng-smart-shrinking
        COMMAND wkhtmltopdf-ng --enable-smart-shrinking
            --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/wide.html
            ${CMAKE_CURRENT_BINARY_DIR}/wide.pdf)
    add_test(NAME wkhtmltopdf-ng-links
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/links.html
            ${CMAKE_CURRENT_BINARY_DIR}/links.pdf)
    add_test(NAME wkhtmltopdf-ng-links-no-external
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            --disable-external-links
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/links.html
            ${CMAKE_CURRENT_BINARY_DIR}/links-no-external.pdf)
    add_test(NAME wkhtmltopdf-ng-links-no-internal
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            --disable-internal-links
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/links.html
            ${CMAKE_CURRENT_BINARY_DIR}/links-no-internal.pdf)
    add_test(NAME wkhtmltopdf-ng-links-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/links.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/links-check)
    add_test(NAME wkhtmltopdf-ng-bank-statement
        COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
            --margin-top 12mm --margin-bottom 14mm
            --header-center "Northridge Bank · [page]/[topage]"
            --footer-left "STMT-2026-07-4412"
            --footer-right "Page [page] of [topage]"
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/bank-statement.html
            ${CMAKE_CURRENT_BINARY_DIR}/bank-statement.pdf)
    add_test(NAME wkhtmltopdf-ng-bank-statement-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/bank_statement.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/bank-statement-check)
    add_test(NAME wkhtmltopdf-ng-js-font
        COMMAND wkhtmltopdf-ng --enable-local-file-access --no-stop-slow-scripts
            --javascript-delay 200 --window-status js-font-done
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/js-font.html
            ${CMAKE_CURRENT_BINARY_DIR}/js-font.pdf)
    add_test(NAME wkhtmltopdf-ng-js-font-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/js_font.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/js-font-check)
    add_test(NAME wkhtmltopdf-ng-js-images
        COMMAND wkhtmltopdf-ng --enable-local-file-access --no-stop-slow-scripts
            --javascript-delay 200 --window-status js-images-done
            --load-media-error-handling ignore
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/js-images.html
            ${CMAKE_CURRENT_BINARY_DIR}/js-images.pdf)
    add_test(NAME wkhtmltopdf-ng-js-images-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/js_images.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/js-images-check)
    add_test(NAME wkhtmltopdf-ng-heavy-cdn-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/heavy_cdn.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/heavy-cdn-check)
    add_test(NAME wkhtmltopdf-ng-modern-css-js
        COMMAND wkhtmltopdf-ng --enable-local-file-access --no-stop-slow-scripts
            --javascript-delay 200 --window-status modern-css-js-done
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern-css-js.html
            ${CMAKE_CURRENT_BINARY_DIR}/modern-css-js.pdf)
    add_test(NAME wkhtmltopdf-ng-modern-css-js-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/modern_css_js.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/modern-css-js-check)
    add_test(NAME wkhtmltopdf-ng-print-layout-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/print_layout.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/print-layout-check)
    add_test(NAME wkhtmltopdf-ng-remote-images-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/remote_images.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/remote-images-check)
    add_test(NAME wkhtmltopdf-ng-reg-next-check
        COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/reg_next.sh
            $<TARGET_FILE:wkhtmltopdf-ng>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            ${CMAKE_CURRENT_BINARY_DIR}/reg-next-check)
    set_tests_properties(wkhtmltopdf-ng-dpi wkhtmltopdf-ng-smart-shrinking
        wkhtmltopdf-ng-links wkhtmltopdf-ng-links-no-external
        wkhtmltopdf-ng-links-no-internal wkhtmltopdf-ng-links-check
        wkhtmltopdf-ng-bank-statement wkhtmltopdf-ng-bank-statement-check
        wkhtmltopdf-ng-js-font wkhtmltopdf-ng-js-font-check
        wkhtmltopdf-ng-js-images wkhtmltopdf-ng-js-images-check
        wkhtmltopdf-ng-heavy-cdn-check
        wkhtmltopdf-ng-modern-css-js wkhtmltopdf-ng-modern-css-js-check
        wkhtmltopdf-ng-print-layout-check wkhtmltopdf-ng-remote-images-check
        wkhtmltopdf-ng-reg-next-check
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)
    set_tests_properties(wkhtmltopdf-ng-links-check
        wkhtmltopdf-ng-bank-statement-check
        wkhtmltopdf-ng-js-font-check wkhtmltopdf-ng-js-images-check
        wkhtmltopdf-ng-heavy-cdn-check wkhtmltopdf-ng-modern-css-js-check
        wkhtmltopdf-ng-print-layout-check wkhtmltopdf-ng-remote-images-check
        wkhtmltopdf-ng-reg-next-check
        PROPERTIES TIMEOUT 180)

    add_test(NAME wkhtmltoimage-ng-png
        COMMAND wkhtmltoimage-ng --enable-local-file-access
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern.html
            ${CMAKE_CURRENT_BINARY_DIR}/modern.png)
    add_test(NAME wkhtmltoimage-ng-crop
        COMMAND wkhtmltoimage-ng --enable-local-file-access
            --crop-width 400 --crop-height 300
            ${CMAKE_CURRENT_SOURCE_DIR}/test_data/modern.html
            ${CMAKE_CURRENT_BINARY_DIR}/modern-crop.jpg)
    set_tests_properties(wkhtmltoimage-ng-png
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)
    set_tests_properties(wkhtmltoimage-ng-crop
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
            TIMEOUT 60)

    find_program(WKHTMLTOPDF_NG_QPDF qpdf)
    if(WKHTMLTOPDF_NG_QPDF)
        add_test(NAME wkhtmltopdf-ng-native-outline
            COMMAND wkhtmltopdf-ng --outline --dump-outline
                ${CMAKE_CURRENT_BINARY_DIR}/outline.xml
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
                ${CMAKE_CURRENT_BINARY_DIR}/outline.pdf)
        add_test(NAME wkhtmltopdf-ng-page-counters
            COMMAND wkhtmltopdf-ng --header-center "[page]/[topage]"
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
                ${CMAKE_CURRENT_BINARY_DIR}/page-counters.pdf)
        add_test(NAME wkhtmltopdf-ng-html-header
            COMMAND wkhtmltopdf-ng --header-html
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/header.html
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
                ${CMAKE_CURRENT_BINARY_DIR}/html-header.pdf)
        add_test(NAME wkhtmltopdf-ng-header-footer-10pages
            COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
                --margin-top 22mm --margin-bottom 22mm
                --header-html ${CMAKE_CURRENT_SOURCE_DIR}/test_data/header-repeat.html
                --footer-html ${CMAKE_CURRENT_SOURCE_DIR}/test_data/footer-repeat.html
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/ten-pages.html
                ${CMAKE_CURRENT_BINARY_DIR}/header-footer-10.pdf)
        add_test(NAME wkhtmltopdf-ng-header-footer-check
            COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/header_footer.sh
                $<TARGET_FILE:wkhtmltopdf-ng>
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data
                ${CMAKE_CURRENT_BINARY_DIR}/header-footer-check)
        add_test(NAME wkhtmltopdf-ng-pagination
            COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
                --title "Pagination Example"
                --margin-top 22mm --margin-bottom 22mm
                --header-html ${CMAKE_CURRENT_SOURCE_DIR}/test_data/pagination-header.html
                --footer-html ${CMAKE_CURRENT_SOURCE_DIR}/test_data/pagination-footer.html
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/pagination.html
                ${CMAKE_CURRENT_BINARY_DIR}/pagination.pdf)
        add_test(NAME wkhtmltopdf-ng-pagination-check
            COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/pagination.sh
                $<TARGET_FILE:wkhtmltopdf-ng>
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data
                ${CMAKE_CURRENT_BINARY_DIR}/pagination-check)
        add_test(NAME wkhtmltopdf-ng-per-object-page-numbers
            COMMAND wkhtmltopdf-ng --javascript-delay 0 --enable-local-file-access
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/toc.html
                --header-center "FIRST [page]/[topage]"
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data/multi-second.html
                --header-center "SECOND [page]/[topage]"
                ${CMAKE_CURRENT_BINARY_DIR}/per-object-page-numbers.pdf)
        add_test(NAME wkhtmltopdf-ng-per-object-page-numbers-check
            COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/per_object_pages.sh
                $<TARGET_FILE:wkhtmltopdf-ng>
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data
                ${CMAKE_CURRENT_BINARY_DIR}/per-object-pages-check)
        add_test(NAME wkhtmltopdf-ng-extra-features-check
            COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/extra_features.sh
                $<TARGET_FILE:wkhtmltopdf-ng>
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data
                ${CMAKE_CURRENT_BINARY_DIR}/extra-features-check)
        add_test(NAME wkhtmltopdf-ng-pdf-edit-check
            COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/pdf_edit.sh
                $<TARGET_FILE:wkhtmltopdf-ng>
                ${CMAKE_CURRENT_SOURCE_DIR}/test_data
                ${CMAKE_CURRENT_BINARY_DIR}/pdf-edit-check)
        set_tests_properties(wkhtmltopdf-ng-native-outline wkhtmltopdf-ng-page-counters
            wkhtmltopdf-ng-html-header wkhtmltopdf-ng-header-footer-10pages
            wkhtmltopdf-ng-header-footer-check wkhtmltopdf-ng-pagination
            wkhtmltopdf-ng-pagination-check wkhtmltopdf-ng-per-object-page-numbers
            wkhtmltopdf-ng-per-object-page-numbers-check wkhtmltopdf-ng-pdf-edit-check
            PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox"
                TIMEOUT 60)
        set_tests_properties(wkhtmltopdf-ng-header-footer-check
            wkhtmltopdf-ng-pagination-check
            wkhtmltopdf-ng-per-object-page-numbers-check
            wkhtmltopdf-ng-extra-features-check
            wkhtmltopdf-ng-pdf-edit-check PROPERTIES TIMEOUT 120)
    endif()
endif()

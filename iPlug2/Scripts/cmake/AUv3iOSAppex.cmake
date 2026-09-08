#  ==============================================================================
#
#  This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.
#
#  See LICENSE.txt for  more info.
#
#  ==============================================================================

# AUv3 Appex target configuration for iPlug2 iOS
# The appex is an app extension (executable) that hosts the framework

include(${CMAKE_CURRENT_LIST_DIR}/AUv3iOS.cmake)

function(iplug_configure_auv3iosappex target project_name)
  set(framework_target ${project_name}AU-ios-framework)

  # Appex is an app bundle (executable) with .appex extension
  # Uses NSExtensionMain entry point from Foundation
  # For Ninja: Use separate output dir to avoid collision with APP target
  if(XCODE)
    set(APPEX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
  else()
    set(APPEX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out/appex-build")
  endif()

  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${PLUG_RESOURCES_DIR}/${project_name}-iOS-AUv3-Info.plist
    RUNTIME_OUTPUT_DIRECTORY "${APPEX_OUTPUT_DIR}"
    OUTPUT_NAME "${project_name}AppExtension"
    # iOS-specific Xcode attributes
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION "appex"
    XCODE_PRODUCT_TYPE "com.apple.product-type.app-extension"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "ZUGJCP8877"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.thesecretlaboratory.apps.${project_name}.AUv3"
    XCODE_ATTRIBUTE_SKIP_INSTALL "YES"
    XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY "YES"
  )

  # Use NSExtensionMain as entry point (provided by Foundation)
  # Add rpath to find framework at App/Frameworks/ (one level up from appex in PlugIns/)
  # iOS bundle structure: App/PlugIns/appex -> App/Frameworks/
  target_link_options(${target} PRIVATE
    "-e" "_NSExtensionMain"
    "-Wl,-rpath,@executable_path/../../Frameworks"
  )

  # Appex links to the framework binary at build time
  # At runtime, it finds the framework via rpath in the host app
  add_dependencies(${target} ${framework_target})

  # Link to the framework
  target_link_libraries(${target} PRIVATE
    "$<TARGET_FILE:${framework_target}>"
    "-framework Foundation"
  )

  # Enable ARC for appex source
  set(APPEX_SOURCE ${PLUG_RESOURCES_DIR}/${project_name}AUv3Appex.m)
  if(EXISTS ${APPEX_SOURCE})
    set_source_files_properties(${APPEX_SOURCE}
      PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
  endif()

  # Post-build: Create PkgInfo and rename bundle
  set(PKGINFO_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/write_pkginfo_${target}.cmake")
  file(WRITE ${PKGINFO_SCRIPT} "file(WRITE \"\${PKGINFO_PATH}\" \"XPC!????\")")

  if(XCODE)
    # For Xcode: XCODE_ATTRIBUTE_WRAPPER_EXTENSION creates .appex bundle directly
    # Bundle is created as ${project_name}AppExtension.appex, we rename to ${project_name}AUv3.appex
    set(APPEX_BUNDLE "${CMAKE_BINARY_DIR}/out/$<CONFIG>/${project_name}AppExtension.appex")
    set(APPEX_FINAL "${CMAKE_BINARY_DIR}/out/$<CONFIG>/${project_name}AUv3.appex")
    add_custom_command(TARGET ${target} POST_BUILD
      # Create PkgInfo
      COMMAND ${CMAKE_COMMAND} -DPKGINFO_PATH="${APPEX_BUNDLE}/PkgInfo"
        -P "${PKGINFO_SCRIPT}"
      # Copy Info.plist into bundle (no rename - let Xcode sign as-is)
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PLUG_RESOURCES_DIR}/${project_name}-iOS-AUv3-Info.plist"
        "${APPEX_BUNDLE}/Info.plist"
      COMMENT "Building ${project_name}AUv3.appex (iOS)"
    )
  else()
    # For Ninja/other generators: Convert .app to .appex and move to final location
    add_custom_command(TARGET ${target} POST_BUILD
      # Create PkgInfo
      COMMAND ${CMAKE_COMMAND} -DPKGINFO_PATH="$<TARGET_BUNDLE_DIR:${target}>/PkgInfo"
        -P "${PKGINFO_SCRIPT}"
      # Copy Info.plist
      COMMAND ${CMAKE_COMMAND} -E copy
        "${PLUG_RESOURCES_DIR}/${project_name}-iOS-AUv3-Info.plist"
        "$<TARGET_BUNDLE_DIR:${target}>/Info.plist"
      # Move and rename .app bundle to .appex in final location
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/out/${project_name}AUv3.appex"
      COMMAND ${CMAKE_COMMAND} -E rename
        "$<TARGET_BUNDLE_DIR:${target}>"
        "${CMAKE_BINARY_DIR}/out/${project_name}AUv3.appex"
      COMMENT "Building ${project_name}AUv3.appex (iOS)"
    )
  endif()
endfunction()

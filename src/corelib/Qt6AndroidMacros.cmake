# Generate deployment tool json

# Locate newest Android sdk build tools revision
function(_qt_internal_android_get_sdk_build_tools_revision out_var)
    if (NOT QT_ANDROID_SDK_BUILD_TOOLS_REVISION)
        file(GLOB android_build_tools
            LIST_DIRECTORIES true
            RELATIVE "${ANDROID_SDK_ROOT}/build-tools"
            "${ANDROID_SDK_ROOT}/build-tools/*")
        if (NOT android_build_tools)
            message(FATAL_ERROR "Could not locate Android SDK build tools under \"${ANDROID_SDK_ROOT}/build-tools\"")
        endif()
        list(SORT android_build_tools)
        list(REVERSE android_build_tools)
        list(GET android_build_tools 0 android_build_tools_latest)
    endif()
    set(${out_var} "${android_build_tools_latest}" PARENT_SCOPE)
endfunction()

# Generate the deployment settings json file for a cmake target.
function(qt6_android_generate_deployment_settings target)
    # Information extracted from mkspecs/features/android/android_deployment_settings.prf
    if (NOT TARGET ${target})
        message(FATAL_ERROR "${target} is not a cmake target")
        return()
    endif()

    get_target_property(target_type ${target} TYPE)

    if (NOT "${target_type}" STREQUAL "MODULE_LIBRARY")
        message(SEND_ERROR "QT_ANDROID_GENERATE_DEPLOYMENT_SETTINGS only works on Module targets")
        return()
    endif()

    get_target_property(target_source_dir ${target} SOURCE_DIR)
    get_target_property(target_binary_dir ${target} BINARY_DIR)
    get_target_property(target_output_name ${target} OUTPUT_NAME)
    if (NOT target_output_name)
        set(target_output_name ${target})
    endif()
    set(deploy_file "${target_binary_dir}/android-${target_output_name}-deployment-settings.json")

    set(file_contents "{\n")
    # content begin
    string(APPEND file_contents
        "   \"description\": \"This file is generated by cmake to be read by androiddeployqt and should not be modified by hand.\",\n")

    # Host Qt Android install path
    if (NOT QT_BUILDING_QT OR QT_STANDALONE_TEST_PATH)
        set(qt_path "${QT6_INSTALL_PREFIX}")
        set(android_plugin_dir_path "${qt_path}/${QT6_INSTALL_PLUGINS}/platforms")
        set(glob_expression "${android_plugin_dir_path}/*qtforandroid*${CMAKE_ANDROID_ARCH_ABI}.so")
        file(GLOB plugin_dir_files LIST_DIRECTORIES FALSE "${glob_expression}")
        if (NOT plugin_dir_files)
            message(SEND_ERROR
                "Detected Qt installation does not contain qtforandroid_${CMAKE_ANDROID_ARCH_ABI}.so in the following dir:\n"
                "${android_plugin_dir_path}\n"
                "This is most likely due to the installation not being a Qt for Android build. "
                "Please recheck your build configuration.")
            return()
        else()
            list(GET plugin_dir_files 0 android_platform_plugin_path)
            message(STATUS "Found android platform plugin at: ${android_platform_plugin_path}")
        endif()
    endif()

    set(qt_android_install_dir "${QT6_INSTALL_PREFIX}")
    file(TO_CMAKE_PATH "${qt_android_install_dir}" qt_android_install_dir_native)
    string(APPEND file_contents
        "   \"qt\": \"${qt_android_install_dir_native}\",\n")

    # Android SDK path
    file(TO_CMAKE_PATH "${ANDROID_SDK_ROOT}" android_sdk_root_native)
    string(APPEND file_contents
        "   \"sdk\": \"${android_sdk_root_native}\",\n")

    # Android SDK Build Tools Revision
    get_target_property(android_sdk_build_tools ${target} QT_ANDROID_SDK_BUILD_TOOLS_REVISION)
    if (NOT android_sdk_build_tools)
        _qt_internal_android_get_sdk_build_tools_revision(android_sdk_build_tools)
    endif()
    string(APPEND file_contents
        "   \"sdkBuildToolsRevision\": \"${android_sdk_build_tools}\",\n")

    # Android NDK
    file(TO_CMAKE_PATH "${CMAKE_ANDROID_NDK}" android_ndk_root_native)
    string(APPEND file_contents
        "   \"ndk\": \"${android_ndk_root_native}\",\n")

    # Setup LLVM toolchain
    string(APPEND file_contents
        "   \"toolchain-prefix\": \"llvm\",\n")
    string(APPEND file_contents
        "   \"tool-prefix\": \"llvm\",\n")
    string(APPEND file_contents
        "   \"useLLVM\": true,\n")

    # NDK Toolchain Version
    string(APPEND file_contents
        "   \"toolchain-version\": \"${CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION}\",\n")

    # NDK Host
    string(APPEND file_contents
        "   \"ndk-host\": \"${ANDROID_NDK_HOST_SYSTEM_NAME}\",\n")

    if (CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
        set(arch_value "i686-linux-android")
    elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
        set(arch_value "x86_64-linux-android")
    elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        set(arch_value "aarch64-linux-android")
    else()
        set(arch_value "arm-linux-androideabi")
    endif()

    # Architecture
    string(APPEND file_contents
        "   \"architectures\": { \"${CMAKE_ANDROID_ARCH_ABI}\" : \"${arch_value}\" },\n")

    # deployment dependencies
    get_target_property(android_deployment_dependencies
        ${target} QT_ANDROID_DEPLOYMENT_DEPENDENCIES)
    if (android_deployment_dependencies)
        list(JOIN android_deployment_dependencies "," android_deployment_dependencies)
        string(APPEND file_contents
            "   \"deployment-dependencies\": \"${android_deployment_dependencies}\",\n")
    endif()

    # Extra plugins
    get_target_property(android_extra_plugins ${target} QT_ANDROID_EXTRA_PLUGINS)
    if (android_extra_plugins)
        list(JOIN android_extra_plugins "," android_extra_plugins)
        string(APPEND file_contents
            "   \"android-extra-plugins\": \"${android_extra_plugins}\",\n")
    endif()

    # Extra libs
    get_target_property(android_extra_libs ${target} QT_ANDROID_EXTRA_LIBS)
    if (android_extra_libs)
        list(JOIN android_extra_libs "," android_extra_libs)
        string(APPEND file_contents
            "   \"android-extra-libs\": \"${android_extra_libs}\",\n")
    endif()

    # package source dir
    get_target_property(android_package_source_dir ${target} QT_ANDROID_PACKAGE_SOURCE_DIR)
    if (android_package_source_dir)
        file(TO_CMAKE_PATH "${android_package_source_dir}" android_package_source_dir_native)
        string(APPEND file_contents
            "   \"android-package-source-directory\": \"${android_package_source_dir_native}\",\n")
    endif()

    # version code
    get_target_property(android_version_code ${target} QT_ANDROID_VERSION_CODE)
    if (android_version_code)
        string(APPEND file_contents
            "   \"android-version-code\": \"${android_version_code}\",\n")
    endif()

    # version name
    get_target_property(android_version_name ${target} QT_ANDROID_VERSION_NAME)
    if (android_version_name)
        string(APPEND file_contents
            "   \"android-version-name\": \"${android_version_name}\",\n")
    endif()

    # minimum SDK version
    get_target_property(android_min_sdk_version ${target} QT_ANDROID_MIN_SDK_VERSION)
    if(android_min_sdk_version)
        string(APPEND file_contents
            "   \"android-min-sdk-version\": \"${android_min_sdk_version}\",\n")
    endif()

    # target SDK version
    get_target_property(android_target_sdk_version ${target} QT_ANDROID_TARGET_SDK_VERSION)
    if(android_target_sdk_version)
        string(APPEND file_contents
            "   \"android-target-sdk-version\": \"${android_target_sdk_version}\",\n")
    endif()

    get_target_property(qml_import_path ${target} QT_QML_IMPORT_PATH)
    if (qml_import_path)
        set(_import_paths "")
        foreach(_path IN LISTS qml_import_path)
            file(TO_CMAKE_PATH "${_path}" _path)
            list(APPEND _import_paths ${_path})
        endforeach()
        list(JOIN _import_paths "," _import_paths)
        string(APPEND file_contents
            "   \"qml-import-paths\": \"${_import_paths}\",\n")
    endif()

    get_target_property(qml_root_paths ${target} QT_QML_ROOT_PATH)
    if(NOT qml_root_paths)
        set(qml_root_paths "${target_source_dir}")
    endif()

    set(qml_native_root_paths "")
    foreach(root_path IN LISTS qml_root_paths)
        file(TO_CMAKE_PATH "${root_path}" qml_root_path_native)
        list(APPEND qml_native_root_paths "\"${qml_root_path_native}\"")
    endforeach()

    list(JOIN qml_native_root_paths "," qml_native_root_paths)

    string(APPEND file_contents
        "   \"qml-root-path\": [${qml_native_root_paths}],\n")

    # App binary
    string(APPEND file_contents
        "   \"application-binary\": \"${target_output_name}\",\n")

    # App command-line arguments
    if (QT_ANDROID_APPLICATION_ARGUMENTS)
        string(APPEND file_contents
            "   \"android-application-arguments\": \"${QT_ANDROID_APPLICATION_ARGUMENTS}\",\n")
    endif()

    # Override qmlimportscanner binary path
    set(qml_importscanner_binary_path "${QT_HOST_PATH}/${QT6_HOST_INFO_LIBEXECDIR}/qmlimportscanner")
    if (WIN32)
        string(APPEND qml_importscanner_binary_path ".exe")
    endif()
    file(TO_CMAKE_PATH "${qml_importscanner_binary_path}" qml_importscanner_binary_path_native)
    string(APPEND file_contents
        "   \"qml-importscanner-binary\" : \"${qml_importscanner_binary_path_native}\",\n")

    # Override rcc binary path
    set(rcc_binary_path "${QT_HOST_PATH}/${QT6_HOST_INFO_LIBEXECDIR}/rcc")
    if (WIN32)
        string(APPEND rcc_binary_path ".exe")
    endif()
    file(TO_CMAKE_PATH "${rcc_binary_path}" rcc_binary_path_native)
    string(APPEND file_contents
        "   \"rcc-binary\" : \"${rcc_binary_path_native}\",\n")

    # Extra prefix paths
    foreach(prefix IN LISTS CMAKE_FIND_ROOT_PATH)
        if (NOT "${prefix}" STREQUAL "${qt_android_install_dir_native}"
            AND NOT "${prefix}" STREQUAL "${android_ndk_root_native}")
            list(APPEND extra_prefix_list \"${prefix}\")
        endif()
    endforeach()
    string (REPLACE ";" "," extra_prefix_list "${extra_prefix_list}")
    string(APPEND file_contents
        "   \"extraPrefixDirs\" : [ ${extra_prefix_list} ],\n")

    if(QT_FEATURE_zstd)
        set(is_zstd_enabled "true")
    else()
        set(is_zstd_enabled "false")
    endif()
    string(APPEND file_contents
        "   \"zstdCompression\": ${is_zstd_enabled},\n")

    # Last item in json file

    # base location of stdlibc++, will be suffixed by androiddeploy qt
    # Sysroot is set by Android toolchain file and is composed of ANDROID_TOOLCHAIN_ROOT.
    set(android_ndk_stdlib_base_path "${CMAKE_SYSROOT}/usr/lib/")
    string(APPEND file_contents
        "   \"stdcpp-path\": \"${android_ndk_stdlib_base_path}\"\n")

    # content end
    string(APPEND file_contents "}\n")

    file(WRITE ${deploy_file} ${file_contents})

    set_target_properties(${target}
        PROPERTIES
            QT_ANDROID_DEPLOYMENT_SETTINGS_FILE ${deploy_file}
    )
endfunction()

if(NOT QT_NO_CREATE_VERSIONLESS_FUNCTIONS)
    function(qt_android_generate_deployment_settings)
        qt6_android_generate_deployment_settings(${ARGV})
    endfunction()
endif()

function(qt6_android_apply_arch_suffix target)
    get_target_property(target_type ${target} TYPE)
    if (target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY")
        set_property(TARGET "${target}" PROPERTY SUFFIX "_${CMAKE_ANDROID_ARCH_ABI}.so")
    elseif (target_type STREQUAL "STATIC_LIBRARY")
        set_property(TARGET "${target}" PROPERTY SUFFIX "_${CMAKE_ANDROID_ARCH_ABI}.a")
    endif()
endfunction()

if(NOT QT_NO_CREATE_VERSIONLESS_FUNCTIONS)
    function(qt_android_apply_arch_suffix)
        qt6_android_apply_arch_suffix(${ARGV})
    endfunction()
endif()

# Add custom target to package the APK
function(qt6_android_add_apk_target target)
    get_target_property(deployment_file ${target} QT_ANDROID_DEPLOYMENT_SETTINGS_FILE)
    if (NOT deployment_file)
        message(FATAL_ERROR "Target ${target} is not a valid android executable target\n")
    endif()

    # Make global apk target depend on the current apk target.
    if(TARGET apk)
        add_dependencies(apk ${target}_make_apk)
        _qt_internal_create_global_apk_all_target_if_needed()
    endif()

    set(deployment_tool "${QT_HOST_PATH}/${QT6_HOST_INFO_BINDIR}/androiddeployqt")
    set(apk_final_dir "$<TARGET_PROPERTY:${target},BINARY_DIR>/android-build")
    set(apk_intermediate_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build")
    set(apk_file_name "${target}.apk")
    set(dep_file_name "${target}.d")
    set(apk_final_file_path "${apk_final_dir}/${apk_file_name}")
    set(apk_intermediate_file_path "${apk_intermediate_dir}/${apk_file_name}")
    set(dep_intermediate_file_path "${apk_intermediate_dir}/${dep_file_name}")

    # This target is used by Qt Creator's Android support and by the ${target}_make_apk target
    # in case DEPFILEs are not supported.
    add_custom_target(${target}_prepare_apk_dir ALL
        DEPENDS ${target}
        COMMAND ${CMAKE_COMMAND}
            -E copy_if_different $<TARGET_FILE:${target}>
            "${apk_final_dir}/libs/${CMAKE_ANDROID_ARCH_ABI}/$<TARGET_FILE_NAME:${target}>"
        COMMENT "Copying ${target} binary to apk folder"
    )

    # The DEPFILE argument to add_custom_command is only available with Ninja or CMake>=3.20 and make.
    if (CMAKE_GENERATOR MATCHES "Ninja" OR
        (CMAKE_VERSION VERSION_GREATER_EQUAL 3.20 AND CMAKE_GENERATOR MATCHES "Makefiles"))
        # Add custom command that creates the apk in an intermediate location.
        # We need the intermediate location, because we cannot have target-dependent generator
        # expressions in OUTPUT.
        add_custom_command(OUTPUT "${apk_intermediate_file_path}"
            COMMAND ${CMAKE_COMMAND}
                -E copy "$<TARGET_FILE:${target}>"
                "${apk_intermediate_dir}/libs/${CMAKE_ANDROID_ARCH_ABI}/$<TARGET_FILE_NAME:${target}>"
            COMMAND "${deployment_tool}"
                --input "${deployment_file}"
                --output "${apk_intermediate_dir}"
                --apk "${apk_intermediate_file_path}"
                --depfile "${dep_intermediate_file_path}"
                --builddir "${CMAKE_BINARY_DIR}"
            COMMENT "Creating APK for ${target}"
            DEPENDS "${target}" "${deployment_file}"
            DEPFILE "${dep_intermediate_file_path}")

        # Create a ${target}_make_apk target to copy the apk from the intermediate to its final
        # location.  If the final and intermediate locations are identical, this is a no-op.
        add_custom_target(${target}_make_apk
            COMMAND "${CMAKE_COMMAND}"
                -E copy_if_different "${apk_intermediate_file_path}" "${apk_final_file_path}"
            DEPENDS "${apk_intermediate_file_path}")
    else()
        add_custom_target(${target}_make_apk
            DEPENDS ${target}_prepare_apk_dir
            COMMAND  ${deployment_tool}
                --input ${deployment_file}
                --output ${apk_final_dir}
                --apk ${apk_final_file_path}
            COMMENT "Creating APK for ${target}"
        )
    endif()
endfunction()

function(_qt_internal_create_global_apk_target)
    # Create a top-level "apk" target for convenience, so that users can call 'ninja apk'.
    # It will trigger building all the apk build targets that are added as part of the project.
    # Allow opting out.
    if(NOT QT_NO_GLOBAL_APK_TARGET)
        if(NOT TARGET apk)
            add_custom_target(apk COMMENT "Building all apks")
        endif()
    endif()
endfunction()

# This function allows deciding whether apks should be built as part of the ALL target at first
# add_executable call point, rather than when the 'apk' target is created as part of the
# find_package(Core) call.
#
# It does so by creating a custom 'apk_all' target as an implementation detail.
#
# This is needed to ensure that the decision is made only when the value of QT_BUILDING_QT is
# available, which is defined in qt_repo_build() -> include(QtSetup), which is included after the
# execution of _qt_internal_create_global_apk_target.
function(_qt_internal_create_global_apk_all_target_if_needed)
    if(TARGET apk AND NOT TARGET apk_all)
        # Some Qt tests helper executables have their apk build process failing.
        # qt_internal_add_executables that are excluded from ALL should also not have apks built
        # for them.
        # Don't build apks by default when doing a Qt build.
        set(skip_add_to_all FALSE)
        if(QT_BUILDING_QT)
            set(skip_add_to_all TRUE)
        endif()

        option(QT_NO_GLOBAL_APK_TARGET_PART_OF_ALL
            "Skip building apks as part of the default 'ALL' target" ${skip_add_to_all})

        set(part_of_all "ALL")
        if(QT_NO_GLOBAL_APK_TARGET_PART_OF_ALL)
            set(part_of_all "")
        endif()

        add_custom_target(apk_all ${part_of_all})
        add_dependencies(apk_all apk)
    endif()
endfunction()

if(NOT QT_NO_CREATE_VERSIONLESS_FUNCTIONS)
    function(qt_android_add_apk_target)
        qt6_android_add_apk_target(${ARGV})
    endfunction()
endif()

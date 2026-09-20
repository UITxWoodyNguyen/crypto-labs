# ==============================================================================
# CompilerWarnings.cmake
# ==============================================================================

function(target_set_compiler_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4             # Mức cảnh báo 4
            /w14242         # Chuyển đổi kiểu có nguy cơ mất dữ liệu
            /w14254         # Chuyển đổi enum
            /w14287         # So sánh unsigned với hằng số âm
            /w14296         # Biểu thức so sánh unsigned với 0
            /w14311         # Con trỏ bị cắt bớt
            /permissive-     # Ép tuân thủ chuẩn C++
            /utf-8          # Đảm bảo mã hóa UTF-8 cho source và console
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow                # Cảnh báo che biến (shadowing)
            -Wnon-virtual-dtor      # Class có hàm ảo nhưng destructor không ảo
            -Wcast-align            # Ép kiểu làm sai lệch căn chỉnh bộ nhớ
            -Wunused                # Biến hoặc hàm không được dùng
            -Woverloaded-virtual
            -fstack-protector-strong# Bật stack canary chống tràn bộ đệm
        )

        # Chỉ kích hoạt _FORTIFY_SOURCE khi có cờ tối ưu hóa (Release/RelWithDebInfo)
        target_compile_definitions(${target_name} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
        )
    endif()
endfunction()
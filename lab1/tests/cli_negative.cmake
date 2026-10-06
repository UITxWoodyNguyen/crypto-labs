# Kiem thu am o muc CLI: moi buoc phai co ma thoat dung ky vong
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
set(ENV{HOME} ${WORK})
function(run expect_nonzero)
  execute_process(COMMAND ${TOOL} ${ARGN} WORKING_DIRECTORY ${WORK}
                  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(expect_nonzero AND rc EQUAL 0)
    message(FATAL_ERROR "expected failure but succeeded: ${ARGN}")
  elseif(NOT expect_nonzero AND NOT rc EQUAL 0)
    message(FATAL_ERROR "expected success but failed (${rc}): ${ARGN}")
  endif()
endfunction()

file(WRITE ${WORK}/msg.txt "hello cli negative tests")
run(0 keygen --bits 256 --out ${WORK}/key.bin)
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/ct.bin)
run(0 decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/ct.bin --out ${WORK}/out.txt)
file(READ ${WORK}/out.txt got)
if(NOT got STREQUAL "hello cli negative tests")
  message(FATAL_ERROR "round trip mismatch")
endif()

# AEAD ma hoa khong co --aead -> tu choi
run(1 encrypt --mode gcm --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# IV sai do dai
run(1 encrypt --mode cbc --key ${WORK}/key.bin --iv 0011 --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# Khoa sai do dai
run(1 encrypt --mode ctr --key-hex 0011223344 --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# Khoa sai -> giai ma that bai
run(1 decrypt --mode gcm --aead --key-hex 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f
    --in ${WORK}/ct.bin --out ${WORK}/bad.txt)
# Tamper ciphertext -> that bai & khong ghi file
file(READ ${WORK}/ct.bin raw HEX)
string(SUBSTRING "${raw}" 0 2 b0)
if(b0 STREQUAL "00")
  string(REGEX REPLACE "^00" "01" raw "${raw}")
else()
  string(REGEX REPLACE "^.." "00" raw "${raw}")
endif()
string(LENGTH "${raw}" n)
set(i 0)
set(bytes "")
file(WRITE ${WORK}/tamper.hex "${raw}")
execute_process(COMMAND ${TOOL} decrypt --mode gcm --aead --key ${WORK}/key.bin
                --text ${raw} --encode hex --meta ${WORK}/ct.bin.json --out ${WORK}/tamper.out
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "tampered ciphertext was accepted")
endif()
if(EXISTS ${WORK}/tamper.out)
  message(FATAL_ERROR "plaintext file written despite auth failure")
endif()
# Nonce reuse -> tu choi
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --iv 000000000000000000000001 --in ${WORK}/msg.txt --out ${WORK}/n1.bin)
run(1 encrypt --mode gcm --aead --key ${WORK}/key.bin --iv 000000000000000000000001 --in ${WORK}/msg.txt --out ${WORK}/n2.bin)
# ECB: file > 16KiB bi chan, co --allow-ecb thi qua
string(REPEAT "A" 20000 big)
file(WRITE ${WORK}/big.txt "${big}")
run(1 encrypt --mode ecb --key ${WORK}/key.bin --in ${WORK}/big.txt --out ${WORK}/e.bin)
run(0 encrypt --mode ecb --allow-ecb --key ${WORK}/key.bin --in ${WORK}/big.txt --out ${WORK}/e.bin)
# Hex hong
run(1 encrypt --mode ctr --key-hex ZZ --in ${WORK}/msg.txt)
message(STATUS "cli_negative: all checks OK")

# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/breno/esp/esp-idf/components/bootloader/subproject"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/tmp"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/src/bootloader-stamp"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/src"
  "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/breno/Downloads/OI/Xadrez/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()

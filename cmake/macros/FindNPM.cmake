#
#  FindNPM.cmake
#  cmake/macros
#
#  Created by Thijs Wenker on 01/23/18.
#  Copyright 2018 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http:#www.apache.org/licenses/LICENSE-2.0.html
#

macro(find_npm)
  find_program(NPM_EXECUTABLE "npm")
endmacro()

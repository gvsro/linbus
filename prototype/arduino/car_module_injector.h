// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CAR_MODULE_INJECTOR_H
#define CAR_MODULE_INJECTOR_H

#include "avr_util.h"
#include "lin_frame.h"
#include "injector_actions.h"

// Control injected signals into linbus framed passed between the master and the
// slave (set/reset selected data bits and adjust the checksum byte). 
//
// The logic is provided as example (it simulates pressing the Sport Mode button
// in my car) and should be modified for each application.
namespace car_module_injector {
  
  // Private state of the injector. Do not use from other files.
  namespace private_ {
    extern boolean injections_enabled;
    
    // True if the current linbus frame is transformed by the injector. Othrwise, the 
    // frame is passed as is.
    extern boolean frame_id_matches;
    
    // Used to calculate the modified frame checksum.
    extern uint16 sum;
    
    // The modified frame checksum byte. 
    extern uint8 checksum;
  }
  
  // ====== These functions should be called from main thread only ================
  
  inline void setInjectionsEnabled(boolean enabled) {
    private_::injections_enabled = enabled;
  }
    
  // ====== These function should be called from lib_decoder ISR only =============

  // Called when the id byte is recieved.
  // Called from lin_decoder's ISR.
  inline void onIsrFrameIdRecieved(uint8 id) {
    private_::frame_id_matches = private_::injections_enabled && (id == 0x8e);
    // NOTE(tal): currently we use linbus checksbum V2 which includes also the id byte. 
    private_::sum = id;
    private_::checksum = 0;
  }

  // Called when a data or checksum byte is sent (but not the sync or id bytes). 
  // The injector uses it to compute the modified frame checksum.
  // Called from lin_decoder's ISR.
  inline void onIsrByteSent(uint8 byte_index, uint8 b) {
    // If this is not a frame we modify then do nothing.
    if (!private_::frame_id_matches) {
      return;
    }
    
    // Collect the sum. Used later to compute the checksum byte.
    private_::sum += b;
    
    // If we just recieved the last data byte, compute the modified frame checksum.
    if (byte_index == (8 - 1)) {
      // Keep adding the high and low bytes until no carry.
      for (;;) {
        const uint8 highByte = (uint8)(private_::sum >> 8);
        if (!highByte) {
          break;  
        }
        // NOTE: this can add additional carry.  
        private_::sum = (private_::sum & 0xff) + highByte; 
      }
      private_::checksum = (uint8)(~private_::sum);
    }
  }

  // Called before sending a data bit of the the data or checksum bytes to get the 
  // transfer function for it.
  // byte_index = 0 for first data byte
  // bit_index = 0 for LSB, 7 for MSB.
  // Called from lin_decoder's ISR.
  inline byte onIsrNextBitAction(uint8 byte_index, uint8 bit_index) {
    if (!private_::frame_id_matches) {
      return injector_actions::COPY_BIT;
    }

    // Handle a bit of one of the data bytes.
    if (byte_index < 8) {
      const boolean atSportModeButtonBit = ((byte_index == 1) && (bit_index == 2));
      return atSportModeButtonBit ? injector_actions::FORCE_BIT_1 : injector_actions::COPY_BIT;
    }
    
    // Handle a checksum bit.
    const boolean checksumBit = private_::checksum & bitMask(bit_index);
    return checksumBit ? injector_actions::FORCE_BIT_1 : injector_actions::FORCE_BIT_0;
    
    // TODO: handle the unexpected case of more than 6 + 1 bytes in the frame. For
    // now we will repeat the checksum byte blindly.
  }  
}  // namepsace car_module_injector

#endif



# CODING STYLE

As of July 25, 2026, VeaOS has officially established the proper coding style for the project. The purpose of this CODING STYLE is to:

1. Create code with easily readable source.
2. Provide information regarding the files.
3. Prevent code from being scattered in the Repository without a clear reason.

Thank you to Linux and ReactOS for the inspiration in establishing this CODING STYLE.

---

## 1. Indentation Formatting
Indentation must be used to clarify the hierarchy and readability of the source code.

*   **Indentation Width:** Use 4 spaces for each indentation level. (Randomly mixing Tabs and Spaces is strictly prohibited).

    **Correct:**
    ```c
    VOID
    VEAPI
    Foo(VOID)
    {
        DoSomething();
    }
    ```

    **Incorrect:**
    ```c
    VOID 
    VEAPI
    Foo(VOID)
    {
    DoSomething();
       OrIndentationLikeThisIsWrong();
                   MustNotBeThisFarFromTheLevel();
    }
    ```

*   **Brace Style:** Use *Allman Style*. Each brace `{` and `}` must have its own line.

    **Correct:**
    ```c
    VOID
    VEAPI
    Foo(VOID)
    {
        DoSomething();
        if (Some)
        {
            
        }
    }
    ```

    **Incorrect:**
    ```c
    VOID
    VEAPI
    Foo(VOID) {
        // Linux style is strictly PROHIBITED
        if (SOME){
         // ^^ especially braces attached to parentheses
        }
    }
    ```

*   **Maximum Depth Level:** VeaOS strictly mandates a maximum of 4 indentation levels for any code function. If your code requires more than 4 levels of depth, immediately break the code down into helpers, reduce your function size, or improve the code logic.

---

## 2. Writing Style
Proper writing makes the code much more comfortable to read.

*   **PascalCase:** Every code element, from Variables to Functions (`typedef` variables must use `ALL_UPPERCASE`), must use `PascalCase` to make the code more comfortable to read and certainly to keep your keyboard from heating up too fast :p.
    *Styles to avoid:*
    1. `camelCase`
    2. `snake_case`

    **Correct:**
    ```c
    VOID
    VEAPI
    AkuCintaHatsuneMiku(
        IN INT Aku,
        OUT INT Miku
    )
    {
        IniBaruBener(Aku, Miku);
    }
    ```

    **Incorrect:**
    ```c
    void veapi aku_cinta_hatsune_miku(int aku, int miku)
    {
        // Even though indentation and braces are correct, this is still prohibited
        ini_salah(aku, miku)
    }
    ```

*   **Variable Definition:** Variable definitions must be longer than 2 letters. And the Variable must have a clear name according to the initial purpose of why the Variable was created. Furthermore, it must follow `PascalCase`.

    **Correct:**
    ```c
    BOOLEAN DidSend;
    INT TotalSend;
    ```

    **Incorrect:**
    ```c
    BOOLEAN A // <- PROHIBITED
    BOOLEAN bA // <- STILL PROHIBITED EVEN IF 2 LETTERS DUE TO camelCase
    ```

*   **Function Definition:** Function definitions also have their own written rules.
    
    *Without Arguments (`VOID`):*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    FunctionName(VOID); // <- MUST PUT VOID IF IT DOES NOT ACCEPT ARGUMENTS/VARIABLES
    ```
    
    *With 1 Argument:*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    FunctionName(IN INT Prefix); // <- 1 argument must still be placed on the same line as the function name
    ```
    
    *With 2 or more arguments:*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    FunctionName(
        IN INT Budi, // <- This also requires a newline if there are 2 variables
        OUT INT Santoso // <- Mandatory newline for each variable
    );
    ```

*   **Macro Preprocessor:** Macro Preprocessors like `#define` must follow these naming rules:
    1. PROHIBITED from using `PascalCase`. Must be `ALL_UPPER_CASE`.
    2. PROHIBITED from placing Macro Preprocessors inside functions (MUST BE OUTSIDE FUNCTIONS).
    3. USE clear and meaningful naming when creating a Macro Preprocessor.
    4. If creating 2 Macros adjacent to each other, align their MACRO EXPAND indentation with the MACRO EXPAND next to it.

    **Example:**
    ```c
    #define APIC_EOI                0xB0
    #define APIC_REG_TPR            0x080
    #define APIC_BASE_ADDRESS       0xFEE00000
    #define IOAPIC_BASE_ADDRESS     0xFEC00000
    #define IOAPIC_REGSEL           0x00
    #define IOAPIC_IOWIN            0x10
    #define APIC_REG_ICR_LOW        0x300
    #define APIC_REG_ICR_HIGH       0x310
    #define APIC_REG_EOI            0x0B0
    ```

*   **Comments:** Comments are mini-documentation placed in the code so that people who want to read it will not be confused about what the code means.
    
    *Basic rules:*
    1. Use `//` for single line or multi-line.
    2. Or you can also use `/* COMMENT */`.
    3. PROHIBITED from commenting out dead-code. If there is dead-code, DELETE IT! Do not leave it there.
    4. Every code block that serves a purpose within a function MUST be given a comment so that others know its intent. Ensure the comments are clear and do not raise many questions.
    5. Do not repeat redundant comments that state the obvious.

    **Correct:**
    ```c
    VOID
    VEAPI
    ApicEnableLapic(VOID)
    {
        // Check if the LAPIC Base Address has been mapped to Virtual Memory
        if (ApicLapicBase == NULL)
        {
            KdPrintf("HCT: LAPIC Base Address has not been mapped!\n\r");
            return;
        }

        /* 
         * Spurious Interrupt Vector Register (SIVR) Configuration:
         * - Bit 8   : APIC Software Enable
         * - Bit 0-7 : Spurious Vector (0xFF / 255)
         */
        ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4];
        Sivr |= 0x100;
        Sivr |= 0x0FF;
        ApicLapicBase[LAPIC_SIVR_OFFSET / 4] = Sivr;

        // Reset TPR to 0 so the hardware receives all Interrupt Levels (IRQL PASSIVE_LEVEL)
        ApicLapicBase[LAPIC_TPR_OFFSET / 4] = 0;
    }
    ```

    **Incorrect:**
    ```c
    VOID
    VEAPI
    ApicEnableLapic(VOID)
    {
        if (ApicLapicBase == NULL)
        {
            return; // No explanation why the function immediately returns
        }

        // ApicLapicBase[0] = 0; <-- PROHIBITED: Dead code left commented out! DELETE IT!

        ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4]; // Get Sivr <-- PROHIBITED: Redundant comment/repeating what is obvious in the code

        Sivr |= 0x100; // Set bit to 100 hex <-- Comment does not explain the PURPOSE (why is it set to 0x100?)
        ApicLapicBase[LAPIC_SIVR_OFFSET / 4] = Sivr;

        // OldApicDisableLogic(); <-- PROHIBITED: Do not leave remnants of old test code
    }
    ```

*   **Pointer Style:** The correct pointer style is attached to the *Variable Name*.

    **Correct:**
    ```c
    VOID *Ajl;
    PVOID *AhciInterruptRoutine;
    ```

    **Incorrect:**
    ```c
    VOID* AduhSalah; // <- WRONG
    PVOID * HarusnyaSalah; // <- ALSO WRONG
    ```

*   **Struct and Enumeration Naming:** Struct names must be `typedef`-ed to `UPPER_CASE` and the struct name must be prefixed with `_` as a mark to avoid conflicts, and the same goes for enums. Every variable inside the struct must be a `PascalCase` variable, same as the `PascalCase` rule mentioned above.

    **Correct:**
    ```c
    typedef struct _VEA_DRIVER_OBJECT
    {
        ULONG DriverSize;
        BOOLEAN IsInitialized;
        PVOID DriverContext;
    } VEA_DRIVER_OBJECT, *PVEA_DRIVER_OBJECT;

    typedef enum _HARDWARE_STATE
    {
        HardwareStateDisabled = 0,
        HardwareStateEnabled  = 1,
        HardwareStateError    = 2
    } HARDWARE_STATE, *PHARDWARE_STATE;
    ```

    **Incorrect:**
    ```c
    // PROHIBITED: Tag does not start with '_', typedef is not ALL_UPPER_CASE, and fields use camelCase/snake_case
    typedef struct vea_driver_object
    {
        ulong driver_size;     // WRONG: Data type & field name are snake_case
        BOOLEAN isInitialized; // WRONG: Field uses camelCase
    } vea_driver_object;
    ```

*   **Basic Types in C Language:** Always use explicit basic types such as `ULONG`, `PULONG`, `VOID`, `PVOID`, `ULONG64`, `PULONG64`, etc. Prohibited from using standard C built-in types because it is prone to errors if VeaOS is ported to another machine.

    **Correct:**
    ```c
    VOID
    VEAPI
    ProcessBuffer(
        IN PVOID BufferAddress,
        IN OUT ULONG BufferSize
    )
    {
        BOOLEAN IsValid = TRUE;
        ULONG Index = 0;
    }
    ```

    **Incorrect:**
    ```c
    void
    veapi
    ProcessBuffer(
        void *bufferAddress,     // PROHIBITED: Using standard C void*
        unsigned long bufferSize // PROHIBITED: Using standard C unsigned long
    )
    {
        bool isValid = true;    // PROHIBITED: Using C99 bool
        int index = 0;          // PROHIBITED: Using standard int
    }
    ```

*   **IN, OUT, OPTIONAL:** Functions that accept arguments must add `IN`, `OUT`, `IN OUT`, or `OPTIONAL` if the argument passed into the function is modified by the inside of the function.

    **Correct:**
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    ApicReadRegister(
        IN ULONG RegisterOffset,
        OUT PULONG RegisterValue,
        IN OPTIONAL PVOID ExtraContext
    );
    ```

    **Incorrect:**
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    ApicReadRegister(
        ULONG RegisterOffset,   // WRONG: No IN / OUT marker
        PULONG RegisterValue,   // WRONG: Unclear whether this variable is only read or written to
        PVOID ExtraContext      // WRONG: No OPTIONAL marker
    );
    ```

*   **Top Section of C and H Files:**
    1. **SPDX License:** Must place the license with the following format:
       ```c
       /* VeaOS SPDX License ----------------------------------------------------
         SPDX-License-Identifier: GPL-2.0-only
         LICENSE     : GNU General Public License v2.0
         FILE        : [FileName.c / FileName.h]
         CREATOR     : [Creator Name / Username]
         MAINTAINER  : [Maintainer Name / Team
                        CAN BE NEW LINE. BUT PLEASE MAKE IT A LIST WITH -]
         PURPOSE     : [Brief explanation of this file's purpose. CAN BE NEW LINE]
        ----- Effective since MM-YYYY til FOREVER ------------------------------- */
       ```
    2. **Header:** 
       For `.c`/`.cpp`, the most mandatory Header to add at the top is `veakrnl.h`. After that, you are free to add any Headers you need.
       For `.h`/`.hpp`, the most mandatory Header to add at the top is `#pragma once` first, skip 1 line down, then `#include <procbind.h>` or `#include "procbind.h"`, after that add any Headers you need.
       
       **Example:**
       *.C:*
       ```c
       #include <veakrnl.h>
       #include <mycoolheader.h>
       ```
       *.H:*
       ```c
       #pragma once
       
       #include "procbind.h"
       #include "whateveryouwant.h"
       ```
    3. **Revision History:** Must place Revision History below the header with the following mandatory format:
       ```c
       /* Revision History ------------------------------------------------------
        * Date          : 26-07-2026
        * Author        : Cool Dev
        * Revision      : Added resetting TPR to IRQL PASSIVE_LEVEL
        *
        * Date          : 25-07-2026
        * Author        : VeaOS Team
        * Revision      : Initial initialization of ApicEnableLapic function
        * --------------------------------------------------------------------- */

/*
 * Copyright (c) 2024 GOODIX.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "uart.h"
#include "los_sem.h"
//#include "custom_config.h"

#if LOSCFG_USE_SHELL

//#define SHELL_UART_ID APP_UART_ID

uint8_t UartGetc(void)
{
    uint8_t ch = 0;

    return ch;
}

#endif // LOSCFG_USE_SHELL
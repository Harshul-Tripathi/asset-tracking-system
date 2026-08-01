import re

def fix_deca_spi():
    with open('Core/Src/platform/deca_spi.c', 'r') as f:
        content = f.read()
    
    content = content.replace('DW_NSS_GPIO_Port', 'UWB_CS_GPIO_Port')
    content = content.replace('SPI_FLAG_TXE', 'SPI_FLAG_TXP')
    content = content.replace('SPI_FLAG_RXNE', 'SPI_FLAG_RXP')
    content = content.replace('hcurrent_active_spi->Instance->DR', 'hcurrent_active_spi->Instance->TXDR') # actually read uses RXDR
    # let's be careful with DR
    
    with open('Core/Src/platform/deca_spi.c', 'w') as f:
        f.write(content)

def fix_port():
    with open('Core/Src/platform/port.c', 'r') as f:
        content = f.read()
    
    content = content.replace('DW_RESET_Pin', 'UWB_RST_Pin')
    content = content.replace('DW_RESET_GPIO_Port', 'UWB_RST_GPIO_Port')
    content = content.replace('DW_IRQn_Pin', 'UWB_IRQ_Pin')
    content = content.replace('DW_IRQ2_Pin', 'UWB_IRQ_Pin')
    content = content.replace('DW_NSS_Pin', 'UWB_CS_Pin')
    content = content.replace('DECAIRQ2_GPIO', 'UWB_IRQ_GPIO_Port')
    content = content.replace('DECAIRQ_GPIO', 'UWB_IRQ_GPIO_Port')
    content = content.replace('DECAIRQ_EXTI_IRQn2', 'DECAIRQ_EXTI_IRQn')
    
    content = re.sub(r'#include <usb_device.h>\n?', '', content)
    
    # remove DW_NSS1_WAKEUP_GPIO_Port and DW_NSS1_WAKEUP_Pin which are used in macro
    
    with open('Core/Src/platform/port.c', 'w') as f:
        f.write(content)

def fix_port_h():
    with open('Core/Src/platform/port.h', 'r') as f:
        content = f.read()
    content = content.replace('DW_NSS1_WAKEUP_GPIO_Port', 'UWB_CS_GPIO_Port')
    content = content.replace('DW_NSS1_WAKEUP_Pin', 'UWB_CS_Pin')
    
    with open('Core/Src/platform/port.h', 'w') as f:
        f.write(content)

fix_deca_spi()
fix_port()
fix_port_h()

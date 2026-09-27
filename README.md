# Servidor de eco

Este readme descreve o funcionamento do servidor.

## Comandos

Os buffers de envio têm 512 bytes e contêm um de dois comandos; o buffer de resposta tem 1024 bytes e contém um byte de status e a resposta ao comando.

### echo (mensagem)

echo recebe um parâmetro (uma mensagem de até 506 bytes não nulos), separado por espaço do comando, e devolve o texto "ECHO: (mensagem)" e um byte de status 0x1.

### quit

quit retorna uma mensagem "quitting...", um byte de status 0x0 e fecha o socket do lado do servidor; ao ler esse byte de status, o cliente fecha o próprio socket e retorna.

## Respostas

### echo

echo[espaço] retorna byte de status 0x1 a mensagem após o espaço, mesmo que vazia. echo sem espaço retorna byte de status 0x2, e uma mensagem de aviso sobre o uso do comando.

### quit

quit retorna retorna byte de status 0x0 e uma mensagem "quitting...".

### NOT SUPPORTED

Qualquer outra mensagem retorna byte de status 0xff e uma mensagem "NOT SUPPORTED".
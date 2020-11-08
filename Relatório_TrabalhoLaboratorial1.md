# 1º Trabalho Laboratorial - RCOM

Diogo Guimarães do Rosário - 201806582  
Henrique Melo Ribeiro - 201806529

# Sumário

Este trabalho foi desenvolvido no âmbito da unidade curricular Redes de computadores. Este projeto consistia no desenvolvimento de uma aplicação 
capaz de transferir dados de um computador para outro através de uma porta série assíncrona. A aplicação é resistente a erros na transmissão dos pacotes de dados
e desconexão da porta série.  
A aplicação foi desenvolvida com sucesso, sendo possível transferir ficheiros entre dois computadores sem qualquer perda de informação.

1.Introdução

Este relatório tem o propósito de expor o modo como a nossa aplicação está organizada bem como o funcionamento desta.  
O objetivo deste trabalho é implementar um protocoolo de ligação de dados especificado no guião do trabalho, de modo a permitir transferência fiável de dados entre dois dispositivos
conectados pela porta série.
Assim o relatório estará organizado da seguinte forma: 
 2. Arquitetura e Estrutura do código -  Demonstração dos blocos funcionais e interfaces e exposição das principais estruturas de dados, funções e sua relação com a arquitetura
 3. Casos de uso principais -   Identificação das sequências de chamada de funções
 4. Protocolo de ligação lógica - Identificação dos principais aspetos funcionais, descrição da estratégia de implementação destes aspetos com apresentação de extratos de código
 5. Protocolo de aplicação -  Identificação dos principais aspetos funcionais, descrição da estratégia de implementação destes aspetos com apresentação de extratos de código
 6. Validação -  Descrição dos testes efetuados com apresentação dos resultados
 7. Eficiência do protocolo de ligação de dados -  Caraterização estatística da eficiência do protocolo
 8. Conclusões -  Síntese da informação apresentada nas secções anteriores
 
 2. Arquitetura e Estrutura de código
 O nosso projeto foi desenvolvido com duas camadas principais (protocolo e aplicação).
 
 2.1 Camada do protocolo
 A camado do protocolo está definida no ficheiro common.h e é a camada de nível mais baixo do nosso programa. É responsável pela comunicação entre os dois computadores através da porta série.
 Para além disso, faz uso dos ficheiros writenoncanonical.c, writenoncanonical.h, noncanonical.c e noncanonical.h.
 Esta separação foi feita devido a certas funções nao serem necessárias dos dois lados do protocolo (escrita e leitura), como por exemplo a função de leitura de dados e a sua máquina de estados.
 IMAGEM DE CÓDIGO DO PROTOCOLO
 
 2.2 Camada de aplicação
 A camada da aplicação está definida nos ficheiros application.c e application.h e é a camada que está imediatamente acima do protocolo.
 Esta camada é responsável pela leitura do ficheiro no lado da escrita e criar os pacotes de dados que serão transmitidos à camada do protocolo.
 IMAGEM DE CÓDIGO DA APLICAÇÃO
 
 3. Casos de uso principais
 A aplicação necessita de diferentes parâmetros dependendo se é recetor ou transmissor.
 Do lado do transmissor é necessário o número da porta série a ser usada, a string "transmitter" e o nome do ficheiro.
 Exemplo : ./application 0 transmitter pinguim.gif
 Do lado do recetor é necessário o número da porta série a ser usada e a string "receiver".
 Exemplo : ./application 0 receiver
 O recetor quando iniciado fica à espera de um transmissor para iniciar a conexão. Após estabelecer esta conexão o ficheiro começa a ser transmitido pelo transmissor em pacotes de dados.
 
 4. Protocolo de ligação lógica
 O protocolo de ligação lógica tem como objetivos:
 Configurar a porta série
 Establecer a conexão entre as duas portas série
 Transferência dos pacotes de dados recebidos após operações de stuffing e destuffing
 Deteção de erros nas transmissões
 

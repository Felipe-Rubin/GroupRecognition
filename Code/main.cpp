/*
 CG 2018/1
 Autores:
 Felipe Pfeifer Rubin: Email: felipe.rubin@acad.pucrs.br
 Ian Aragon Escobar: Mat , Email: ian.escobar@acad.pucrs.br
 */

/*
 Arquivo: main.cpp
 - Realiza operacoes com os grupos (em alto nivel)
 - Responsavel pela parte grafica utilizando OpenGL, GLUT
 */

//MacOS tem localizacao diferente do GLUT
#include <iostream>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

//Standard libs
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

//Project Libs
#include "video.h"

//Armazena os dados de cada  "Entidade" de VideoInfo
typedef struct{
    VideoInfo vi;
    int *hascompany;// hascompany deste video
    int frameposGlobal; //pos do frame atual
    int maxGroupSize; //grupo com maior tamanho
    const char *filename; // Arquivo de dados
    GList *list; //Lista de Grupos
    int currFrame; // currFrame desse Controller
    int prevFrame; //prevFrame desse Controller
}VideoController;

/*
    Variaveis globais que alteram de acordo com o video sendo visualizado
 */
VideoInfo *viGlobal; //Video Atual
GList *globalList; // Lista de grupos do Video Atual
int *hascompany; //Se a pessoa ta em grupo = 1 ou nao = 0, do video atual
int frameposGlobal; // posicao do video atual
int *maxGroupSize; //tamanho maximo de grupos de cada video
int pauseVideo = 0; //Pause no video
int prevFrame = -1; //Frame anterior
int currFrame = 0; //Frame atual(usado em Desenha(), p/ n alterar o framePosGlobal durante o desenho da tela)

int videoselector = 0; //Seletor de video

//Os videos com suas respectivas informacoes
VideoController vc[4];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void recognizeGroups(GList *list)
{
    int i,j,k;
    Frame *f;
    int changed = 0;
    int need_merge_group = 0; //1 se tem q unir dois grupos ou mais, 0 se nao precisa
    i = 0;
    f = &viGlobal->frames[i];
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //Pessoa Sozinha e outra Pessoa em Grupo
PSPG:
    for(j = 0; j < viGlobal->people; j++){
        if(!hascompany[j]){
            Group *aux = list->head;
            while(aux != NULL){
                for(k = 0; k < viGlobal->people; k++){
                    if(aux->presence[k] > 0){
                        if(ispp(j, k, viGlobal)){
                            addToGroup(aux,j);
                            hascompany[j] = 1;
                            break;
                        }
                    }
                }
                aux = aux->next;
                if(hascompany[j]){
                    break;
                }
            }
        }
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //Pessoa Sozinha e outra Pessoa Sozinha
    for(j = 0; j < viGlobal->people; j++){
        if(!hascompany[j]){
            for(k = 0; k < j; k++){
                if(!hascompany[k]){
                    if(ispp(j, k, viGlobal)){
                        int minFrame = viGlobal->personBegin[j];
                        
                        if(minFrame > viGlobal->personBegin[k]) minFrame = viGlobal->personBegin[k];
                        addGroup(list,j,k,viGlobal->people);
                        hascompany[j] = 1;
                        hascompany[k] = 1;
                        changed = 1;
                        break;
                    }
                }
            }
        }
    }
    if(changed == 1){
        changed = 0;
        goto PSPG; //Testa denovo PS + PG = G+
    }

    //Prepara p/ fazer merge dos grupos (Verifica Pessoa de um Grupo c/ Pessoa de outro Grupo)
    for(j = 0; j < viGlobal->people; j++){
        if(hascompany[j]){
            Group *aux = list->head;
            while(aux != NULL){
                if(aux->presence[j] == 0){
                    for(k = 0; k < viGlobal->people;k++){
                        if(aux->presence[k] && ispp(j, k, viGlobal)){
                            addToGroup(aux,j);
                            need_merge_group = 1;
                            break;
                            
                        }
                    }
                }
                aux = aux->next;
            }
        }
    }
    
    if(need_merge_group){
        need_merge_group = 0;
        subsetGroups(list);
    }
    //atualiza info dos grupos
    updateGroups(list);
    //ve os grupos e verifica qual  o maior
    Group *aux = list->head;
    while(aux != NULL){
        if(aux->size > *maxGroupSize) *maxGroupSize = aux->size;
        aux = aux->next;
    }
}


/*
 Computa 4 coisas:
 - Quando o grupo eh criado
 - Quando o grupo se desfaz
 - Quando cada integrante entra no grupo
 - Quando cada integrante sai do grupo
*/
void computeinoutgroups()
{
    Group *aux = globalList->head;
    int groupi = 0;
    int i = 0;
    while(aux != NULL){
        printf("---------Group %d------------\n",groupi);
        for(i = 0; i < viGlobal->people; i++){
            if(aux->presence[i] > 0){
                
                inoutGroup(i,aux,viGlobal,&aux->enteredAt[i],&aux->exitedAt[i]);
                if(aux->bornAt == -1) aux->bornAt = aux->enteredAt[i];
                else if(aux->bornAt > aux->enteredAt[i])aux->bornAt = aux->enteredAt[i];
                
                if(aux->deathAt == -1)aux->deathAt = aux->exitedAt[i];
                else if(aux->deathAt < aux->exitedAt[i])aux->deathAt = aux->exitedAt[i];
            }
        }
        printf("Group %d True existance is [%d,%d]\n",groupi,aux->bornAt,aux->deathAt);
        groupi++;
        aux = aux->next;
    }
    
    
}

void Desenha()
{
    /*
        Faz isso p/ garantir que currFrame nao sera alterado durante a execucao de Desenha()
        Se tiver uma  interrupcao de teclado (que mude algo) enquanto a funcao eh executada, vai dar problema.
     */
    currFrame = frameposGlobal;
    char updatedtitle[100]; //Acho que esse tamanho Eh suficiente
    if(pauseVideo)
        sprintf(updatedtitle,"Video %d Frame %d ||",videoselector%4,currFrame);
    else
        sprintf(updatedtitle,"Video %d Frame %d >",videoselector%4,currFrame);
    glutSetWindowTitle(updatedtitle);

    if(currFrame == prevFrame) return;
    if(currFrame >= viGlobal->length || currFrame < 0) return;
    // Limpa a janela de visualização com a cor preta
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f );
    glEnable(GL_PROGRAM_POINT_SIZE_EXT);
    glPointSize(5);
    glBegin(GL_POINTS);
    
    glColor3f(1,1,1);
    int w;
    Frame f = viGlobal->frames[currFrame];
    //Desenha todas as pessoas que estao presentes neste frame
    for(w = 0; w < viGlobal->people; w++){
        if(f.person[w].valid){ //Eh valido da pra printar
            glVertex2f(f.person[w].point.x, f.person[w].point.y);
        }
    }
    glEnd();
    glFlush();
    Group *aux = globalList->head;
    //Desenha todos os grupos que estao presentes neste frame
    while(aux != NULL){
        if(aux->bornAt > currFrame || aux->deathAt < currFrame){
            aux = aux->next;
            continue;
        }
        glBegin(GL_LINE_STRIP);
        glColor3f(aux->color[0], aux->color[1], aux->color[2]);
        for(w = 0; w < aux->length; w++){
            if(aux->presence[w] > 0 && f.person[w].valid && aux->enteredAt[w] <= currFrame && aux->exitedAt[w] >= currFrame)
                glVertex2f(f.person[w].point.x, f.person[w].point.y);
        }
        glEnd();
        aux = aux->next;
    }
    
    glFlush();

    //Ta em modo video, entao incrementa framePosGlobal e continua executando
    if(!pauseVideo){
        if(frameposGlobal < viGlobal->length-1)
            frameposGlobal++;
        glutPostRedisplay();
    }
    prevFrame = currFrame;
}


/*
 Configura a view dependendo da necessidade de cada video
 */
void Inicializa(void)
{
    // Define a janela de visualização 2D
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /*
     OBS: Inverti o top e o bottom (maxpoint.y e minpoint.y) para que
     A origem fosse no canto inferior esquerdo. Caso contrario, ficaria no superior esquerdo
     */
    //-1 e +1 pq as pessoas ficariam na borda (Ja q o minpoint/maxpoint eh calculado pela menor/maior coord das pessoas)
    gluOrtho2D((GLdouble)viGlobal->minpoint.x-1.0, (GLdouble)viGlobal->maxpoint.x+1.0, (GLdouble)viGlobal->maxpoint.y+1.0, (GLdouble)viGlobal->minpoint.y-1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
    Funcoes do teclado que nao sao normais, e.g. left arrow
*/
void SpecialInput(int key, int x, int y)
{
    switch(key)
    {
        case GLUT_KEY_UP: //Avanca de 10 em 10 Frames
            if(pauseVideo){
                if(frameposGlobal >= viGlobal->length-10)
                    frameposGlobal = viGlobal->length-1;
                else
                    frameposGlobal+=10;
                glutPostRedisplay();
            }
            break;
        case GLUT_KEY_DOWN: //Volta de 10 em 10 Frames
            if(pauseVideo){
                if(frameposGlobal < 10)
                frameposGlobal = 0;
                else
                    frameposGlobal-=10;
                glutPostRedisplay();
            }
            break;
        case GLUT_KEY_LEFT: //Volta de 1 em 1 Frame
            if(pauseVideo){
                if(frameposGlobal == 0)break;
                frameposGlobal--;
                glutPostRedisplay();
            }
            break;
        case GLUT_KEY_RIGHT: //Avanca de 1 em 1 Frame
            if(pauseVideo){
                if(frameposGlobal >= viGlobal->length-1)break;
                frameposGlobal++;
                glutPostRedisplay();
            }
            break;
    }
}

/*
    Callback do teclado de keys normais.
    Keys especiais como left arrow precisam de outra funcao
*/

void Teclado (unsigned char key, int x, int y)
{
    if (key == 27){ //Key = esc
        //Limpa memoria e termina o programa
        for(int i = 0; i < 4; i++){
        printf("Clearing Memory of Video %d\n",i);
            freeVideoInfo(&(vc[i].vi));
            free(vc[i].hascompany);
            freeGList(vc[i].list);
        }
        exit(0);
    }
    if(key >= 48 && key <= 57){// Key = [0-9], pula p/ % do video
        int percent = key - 48;
        if(pauseVideo){
            if(percent == 0)
                frameposGlobal = 0;
            else
                frameposGlobal = floor((viGlobal->length/10) * percent);
            glutPostRedisplay();
        }
    }
    if(key == 32){ //Space  Pausa o Video/ Continua o Video
        pauseVideo = !pauseVideo;
        if(frameposGlobal < viGlobal->length-1)
            frameposGlobal++;
        glutPostRedisplay();
    }
    if(key == 107){ // key k  Muda de Video
        if(pauseVideo){
            vc[videoselector%4].prevFrame = prevFrame;
            vc[videoselector%4].currFrame = currFrame;
            vc[videoselector%4].frameposGlobal = frameposGlobal;
            videoselector++;
            viGlobal = &vc[videoselector%4].vi;
            globalList = vc[videoselector%4].list;
            viGlobal = &vc[videoselector%4].vi;
            prevFrame = vc[videoselector%4].prevFrame - 1;
            currFrame = vc[videoselector%4].currFrame;
            frameposGlobal = vc[videoselector%4].frameposGlobal;
            Inicializa();
            glutPostRedisplay();
        }
    }
}

int main() {
    printf("Beginning...\n");

    vc[0].filename = "/Users/felipe/Documents/Faculdade/5o\ Semestre/Fundamentos\ de\ Computacao\ Grafica/Entrega/Austria01.txt";
    vc[1].filename = "/Users/felipe/Documents/Faculdade/5o\ Semestre/Fundamentos\ de\ Computacao\ Grafica/Entrega/France01.txt";
    vc[2].filename = "/Users/felipe/Documents/Faculdade/5o\ Semestre/Fundamentos\ de\ Computacao\ Grafica/Entrega/Germany01.txt";
    vc[3].filename = "/Users/felipe/Documents/Faculdade/5o\ Semestre/Fundamentos\ de\ Computacao\ Grafica/Entrega/Brazil06.txt";
    
    
    //Le os arquivos e depois reconhece os grupos
    //OBS: Soh fiz de tras p/ frente pras referencias globais ja apontarem pro Video 0
    for(int i = 3; i >= 0; i--){
        printf("-------Initializing Video %d-------\n",i);
        VideoInfo *vi = &vc[i].vi;
        //Faz a primeira leitura do arquivo p/ poder dar malloc depois
        readFileInfo(vi, vc[i].filename);
        //Aloca de acordo com os dados que adquiriu na 1a leitura
        allocateVideoInfo(vi);
        //Agora le uma segunda com uma memoria ja alocada
        readFileData(vi,vc[i].filename);
        
        printf("Pixel x Meter: %d\n",vi->pxm);
        printf("Number of People: %d\n",vi->people);
        for(int j = 0; j < vi->people; j++){
            printf("Person %d Timeline [%d,%d]\n",j,vi->personBegin[j],vi->personEnd[j]);
        }
        printf("Begining Frame: %d\n",vi->begin);
        printf("Number of Frames: %d\n",vi->length);
        printf("MinPoint = (%f,%f)\n",vi->minpoint.x,vi->minpoint.y);
        printf("MaxPoint = (%f,%f)\n",vi->maxpoint.x,vi->maxpoint.y);
        vc[i].list = (GList*)malloc(sizeof(GList));
        vc[i].list->size = 0;
        vc[i].list->head = NULL;
        vc[i].list->tail = NULL;
        globalList = vc[i].list;
        viGlobal = &vc[i].vi;
        vc[i].hascompany = (int*)calloc(viGlobal->people,sizeof(int));
        hascompany = vc[i].hascompany;
        vc[i].currFrame = 0;
        vc[i].prevFrame = -1;
        vc[i].frameposGlobal = 0;
        maxGroupSize = &vc[i].maxGroupSize;
        recognizeGroups(vc[i].list);
        printf("------------Processed Data:---------------\n");
        computeinoutgroups();
        printf("Biggest Group Size: %d\n",vc[i].maxGroupSize);
        printGroups(vc[i].list);
        printf("---------------------------------------------\n");
        
    }
    

    int argc = 0;
    char *argv[] = { (char *)"gl", 0 };
    
    glutInit(&argc,argv);
    
    
    // Define do modo de operação da GLUT
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    
    // Especifica o tamanho inicial em pixels da janela GLUT
    glutInitWindowSize(500, 500);
    
    // Cria a janela passando como argumento o título da mesma
    glutCreateWindow("Trabalho de CG");
    
    // Registra a função callback de redesenho da janela de visualização
    glutDisplayFunc(Desenha);
    
    
    // Registra a função callback para tratamento das teclas ASCII
    glutKeyboardFunc (Teclado);
    glutSpecialFunc(SpecialInput);
    
    // Chama a função responsável por fazer as inicializações
    Inicializa();

    glutMainLoop();
    return 0;
}

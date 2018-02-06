#include <iostream>
#include <fstream>
#include <cmath>
#include <bitset>
// #include "memoria.hpp"
using namespace std;

/* ====================================================
   ========================CLASSES=====================
   ====================================================
*/
class TACache{
public:
    int capacidade;
    int tamLinha;
    int numLinhas;
    int numBitsOffset;
    int* diretorio;
    int** tabela;
    TACache(int c, int l);
    ~TACache();
    int getCapacity(){return this->capacidade;}
    int getLineSize(){return this->tamLinha;}
};

class SACache{
public:
    int capacidade;
    int tamLinha;
    int assoc;
    int numConjuntos;
    int* diretorio;
    TACache** conjuntos;
    SACache(int c, int a, int l);
    ~SACache();
    int getCapacity(){return this->capacidade;}
    int getLineSize(){return this->tamLinha;}
};

class MainMemory{
    private:
        int ramsize;
        int vmsize;
        int totalSize;
        int* mainMemory;
    public:
        MainMemory(int ramsize, int vmsize);
        ~MainMemory();
};
class Cache { 
	friend class Memory;
private:
	SACache l1d;
	SACache l1i;
	SACache l2;
	SACache* l3;
	Cache(SACache l1d, SACache l1i, SACache l2, SACache* l3);
public:
	Cache();
	static Cache createCache(SACache l1d, SACache l1i, SACache l2, SACache* l3);
	static int getCacheData(Cache &c, MainMemory mmem, int address, int * value);
	static int getCacheInstruction(Cache &c, MainMemory mmem, int address, int * value);
	static void setCacheData(Cache &c, MainMemory &mmem, int address, int value);
	static void setCacheInstruction(Cache &c, MainMemory mmem, int address, int value);
	static void fetchCacheData(Cache &c, MainMemory mmem, int adress);
	static void fetchCacheInstruction(Cache &c, MainMemory mmem, int adress);
	static Cache duplicateCache(Cache c);
	
};
class Processor{
private:
	int ncores;
public:
	Memory* coreMemory;
	Processor();
	static Processor createProcessor(Memory mem, int ncores);

};

/* ====================================================
   ==================FUNCOES TACACHE===================
   ====================================================
*/
TACache::TACache(int c = 0, int l = 0){
    //atribuicao da capacidade total do cache
    this->capacidade = c;
    //quantidades de bytes por linha do cache
    this->tamLinha = l;
    //A capacidade total dividida pela quantidade de bytes por linha resulta no numero de linhas
    this->numLinhas = this->capacidade/this->tamLinha;
    this->numBitsOffset = (int) log2((float) this->numLinhas);
    //O numero de bits de offset e dado pelo logaritmo do tamanho do bloco

    diretorio = new int [numLinhas];
    for (int i = 0; i < numLinhas; ++i){
        diretorio[i] = -1;
    }
    tabela = new int*[numLinhas];
    for (int i = 0; i < numLinhas; i++){
        this->tabela[i] = new int[l];
    }
}

TACache::~TACache(){
    delete[] diretorio;
    for (int i = 0; i < numLinhas; i++){
        delete[] tabela [i];
    }
    delete[] tabela;
}

//Funcoes de acesso ao TACache fora da classe principal
//Cria um TACache
static TACache* createTACache(int c, int l){return new TACache(c, l);}
//Retorna a capacidade do TACache
int getTACacheCapacity(TACache* tac){return tac->getCapacity();}

//Retorna o tamanho da linha do TACache
int getTACacheLineSize(TACache* tac){return tac->getLineSize();}

bool getTACacheData(TACache *tac, int address, int* value){
    //cout << "ENDERECO:     " << bitset<32>(address) << endl;
    // cout << "NumBitsOffset: " << tac->numBitsOffset << endl;

    int offset = address << (32 - tac->numBitsOffset);
    // cout << "BITS DELOCADOS " << (32 - tac->numBitsOffset) << endl;
    // cout << "OFFSET 1: " << bitset<32>(offset) << endl;
    offset = (offset >> (32 - tac->numBitsOffset));
    // cout << "DESLOCADOS A DIREITA: " << bitset<32>(offset) << endl;
    // cout << "VALOR AND-A-AND:  " << bitset<32>(valor) << endl;
    offset = offset & ((int) (pow(2, tac->numBitsOffset)) - 1);
    //cout << "OFFSET FINAL: " << bitset<32>(offset) << endl;
    // cout << offset << endl;
    int tag = address >> tac->numBitsOffset;
    tag = tag & ((int) (pow(2, (32 - tac->numBitsOffset)) - 1));
    //cout << "BITS DE TAG:  " << bitset<32>(tag) << endl;
    bool hit = false;
    int linha = 0;
    for (int i = 0; i < tac->numLinhas and (not hit); ++i){
        if(tac->diretorio[i] == tag){
            linha = i;
            hit = true;
        } 
    }
    if(hit)
        *value = tac->tabela[linha][offset];
    return hit;
}

bool setTACacheData(TACache* tac, int address, int value){
    bool hit = getTACacheData(tac, address, NULL);
    //Pegando os bits de offset
    int offset = address << (32 - tac->numBitsOffset);
    offset = (offset >> (32 - tac->numBitsOffset));
    offset = offset & ((int) (pow(2, tac->numBitsOffset)) - 1);
    //Pegando os bits de tag
    int tag = address >> tac->numBitsOffset;
    tag = tag & ((int) (pow(2, (32 - tac->numBitsOffset)) - 1));
    if(hit){
        bool naoAchou = true;
        for (int i = 0; i < tac->numLinhas and naoAchou; ++i){
            if(tac->diretorio[i] == tag){
                tac->tabela[i][offset] = value;
                naoAchou = false;
            } 
        }
    }
    else{
        int pos = -1;
        for (int i = 0; i < tac->numLinhas and (pos == -1); ++i){
            if(tac->diretorio[i] == -1){
                pos = i;
            }
        }
        if(pos != -1){
            tac->diretorio[pos] = tag;
            tac->tabela[pos][offset] = value;
        }
        else{
            cerr << "Nao sei ainda" << endl;
        }
    }
    return hit;
}


/* ====================================================
   ==================FUNCOES SACACHE===================
   ====================================================
*/
SACache::SACache(int c, int a, int l){
    //atribuicao da capacidade total do cache
    this->capacidade = c;
    this->assoc = a;
    //Atribuicao da qunatidade de conjuntos.
    this->numConjuntos = c / (l * a);
    //quantidades de bytes por linha do cache
    this->tamLinha = l;
    //this->numBitsOffset = log2((float) this->numLinhas);
    //O numero de bits de offset e dado pelo logaritmo do tamanho do bloco

    diretorio = new int[numConjuntos];
    for (int i = 0; i < numConjuntos; ++i){
        this->diretorio[i] = -1;
    }
    conjuntos = new TACache*[numConjuntos]();
    for (int i = 0; i < numConjuntos; i++){
        this->conjuntos[i] = createTACache(c, l);
    }
}

SACache::~SACache(){
    delete[] diretorio;
    delete[] conjuntos;
}

//Funcoes SACache
static SACache* createSACache(int c, int a, int l){return new SACache(c, a, l);}

int getSACacheCapacity(SACache* sac){return sac->getCapacity();}

int getSACacheLineSize(SACache* sac){return sac->getLineSize();}

bool getSACacheData(SACache* sac, int address, int* value){
    int numBitsLookup = (int) log2(sac->numConjuntos);
    int numBitsOffset = (int) log2(sac->capacidade/(sac->numConjuntos * sac->assoc));

    //Operacoes bit a bit para o calculo do valor do offset
    int offset = address << (32 - numBitsOffset);
    offset = (offset >> (32 - numBitsOffset));
    offset = offset & ((int) (pow(2, numBitsOffset)) - 1);

    //Operacoes bit a bit para o calculo do valor do lookup
    int lookup = address << (32 - (numBitsOffset + numBitsLookup));
    lookup = lookup >> (32 - (numBitsOffset + numBitsLookup));
    lookup = lookup >> numBitsOffset;
    lookup = lookup & ((int) (pow(2, numBitsLookup) - 1));

    //Operacoes bit a bit para o calculo da tag
    int tag = address >> (numBitsOffset + numBitsLookup);
    tag = tag & ((int) (pow(2, (32 - (numBitsOffset + numBitsLookup))) - 1));

    return getTACacheData(sac->conjuntos[lookup], address, value);
}

bool setSACacheData(SACache* sac, int address, int value){
    int numBitsLookup = (int) log2(sac->numConjuntos);
    int numBitsOffset = (int) log2(sac->capacidade/(sac->numConjuntos * sac->assoc));

    //Operacoes bit a bit para o calculo do valor do lookup
    int lookup = address << (32 - (numBitsOffset + numBitsLookup));
    lookup = lookup >> (32 - (numBitsOffset + numBitsLookup));
    lookup = lookup >> numBitsOffset;
    lookup = lookup & ((int) (pow(2, numBitsLookup) - 1));

    return setTACacheData(sac->conjuntos[lookup], address, value);
}

static SACache* duplicateSACache(SACache* sac){
    SACache *duplicado = new SACache(sac->capacidade, sac->assoc, sac->tamLinha);
    for (int i = 0; i < duplicado->numConjuntos; ++i){
        duplicado->conjuntos[i] = sac->conjuntos[i];
    }
    return duplicado;
}

/* ====================================================
   ==================FUNCOES MEMORIA===================
   ====================================================
*/

MainMemory::MainMemory(int ramsize = 0, int vmsize = 0){
    this->ramsize = ramsize;
    this->vmsize = vmsize;
    this->totalSize = ramsize + vmsize;
    mainMemory = new int[totalSize];
}

MainMemory::~MainMemory(){
    delete[] mainMemory;
}

//Funcao principal
int main(int argc, char const *argv[]){
    SACache *sac = createSACache(32768, 8, 64);

    int value = 0;
    cout << getSACacheData(sac, 24523523, &value) << endl;
    return 0;
}

/* ====================================================
   ==================FUNCOES CACHE===================
   ====================================================
*/


Cache::Cache(SACache l1d, SACache l1i, SACache l2, SACache* l3) {
    this->l1d = l1d;
    this->l1i = l1i;
    this->l2 = l2;
    this->l3 = l3;
}



Cache Cache::createCache(SACache l1d, SACache l1i, SACache l2, SACache* l3) {
    return Cache(l1d, l1i, l2, l3);
}

void Cache::fetchCacheData(Cache &c, MainMemory mmem, int address){
	//Encontra o complemento de dois do tamanho da linha de cada cache
	//Faz um and bit a bit com o address para encontrar o endereço em cada cache.
	
	int addressIl1 = address & (-SACache::getSACacheLineSize(c.l1d));
    int addressIl2 = address & (-SACache::getSACacheLineSize(c.l2));
    int addressIl3 = address & (-SACache::getSACacheLineSize(*c.l3));
    int* linel1 = &mmem.memory[addressIl1];
    int* linel2 = &mmem.memory[addressIl2];
    int* linel3 = &mmem.memory[addressIl3];
    SACache::setSACacheLine(c.l1d, address, linel1);
    SACache::setSACacheLine(c.l2, address, linel2);
    SACache::setSACacheLine(*c.l3, address, linel3);
	
	
	
}
void Cache::fetchCacheInstruction(Cache &c, MainMemory mmem, int address){
	
	int addressIl1 = address & (-SACache::getSACacheLineSize(c.l1d));
    int addressIl2 = address & (-SACache::getSACacheLineSize(c.l2));
    int addressIl3 = address & (-SACache::getSACacheLineSize(*c.l3));
    int* linel1 = &mmem.memory[addressIl1];
    int* linel2 = &mmem.memory[addressIl2];
    int* linel3 = &mmem.memory[addressIl3];
    SACache::setSACacheLine(c.l1i, address, linel1);
    SACache::setSACacheLine(c.l2, address, linel2);
    SACache::setSACacheLine(*c.l3, address, linel3);
	
	
}
void Cache::getCacheData(Cache &c,MainMemory mmem, int address, int* value){
	int ret = 1;
    if(SACache::getSACacheData(c.l1d, address, value)){
        return 1;
    }
    else if((SACache::getSACacheData(c.l2, address, value))){
        ret = 2;
    }
    else if(SACache::getSACacheData(*c.l3, address, value)){
        ret = 3;
    }
    else{
        ret = MainMemory::getMainMemoryData(mmem, address, value);
    }

    if(ret > 0){ //Se o address for valido
        fetchCacheData(c, mmem, address);
    }
    return ret;
	
	
	
}
void Cache::getCacheInstrution(Cache &c, MainMemory mmem int address, int value){
	
	int ret = 1;
    if(SACache::getSACacheData(c.l1i, address, value)){
        return 1;
    }
    else if(SACache::getSACacheData(c.l2, address, value)) {
        ret = 2;
    }
    else if(SACache::getSACacheData(*c.l3, address, value)) {
        ret = 3;
    }
    else{
        ret = MainMemory::getMainMemoryData(mmem, address, value);
    }
    
    if (ret > 0) { // Se o address for valido
        fetchCacheInstruction(c, mmem, address);
    }
    
    return ret;
	
	
}

void Cache::setCacheData(Cache &c, MainMemory &mmem, int address, int value){
    int ret = 0;
    if(SACache::getSACacheData(c.l1d, address, &value)){
        ret = 1;
    }
    else if(SACache::getSACacheData(c.l2, address, &value)){
        ret = 2;
    }
    else if(SACache::getSACacheData(*c.l3, address, &value)){
        ret = 3;
    }
    else{
        ret = MainMemory::getMainMemoryData(mmem, address, &value);
    }
    if(ret > 0){
        fetchCacheData(c, mmem, address);
    }

    SACache::setSACacheData(c.l1d, address, value);
    SACache::setSACacheData(c.l2, address, value);
    SACache::setSACacheData(*c.l3, address, value);
    MainMemory::setMainMemoryData(mmem, address, value);
}

void Cache::setCacheInstruction(Cache &c, MainMemory mmem, int address, int value){
    int ret = 1;
    if (SACache::getSACacheData(c.l1i, address, &value)){
        ret = 1;
    }
    else if(SACache::getSACacheData(c.l2, address, &value)){
        ret = 2;
    }
    else if(SACache::getSACacheData(*c.l3, addlid adressress, &value)) {
        ret = 3;
    }
    else{
        ret = MainMemory::getMainMemoryData(mmem, address, &value);
    }
    
    if(ret > 0) { // for valid adress
        fetchCacheData(c, mmem, address);
    }
    SACache::setSACacheData(c.l1i, address, value);
    SACache::setSACacheData(c.l2, address, value);
    SACache::setSACacheData(*c.l3, address, value);
    MainMemory::setMainMemoryData(mmem, address, value);
}

Cache Cache::duplicateCache(Cache c) {
    SACache* l1d = SACache::duplicateSACache(c.l1d);
    SACache* l1i = SACache::duplicateSACache(c.l1i);
    SACache* l2 = SACache::duplicateSACache(c.l2);
    SACache* l3 = c.l3;
    return Cache::createCache(*l1d,*l1i,*l2,l3);
}

/* ====================================================
   ==================FUNCOES PROCESSOR===================
   ====================================================
*/

Processor Processor::createProcessor(Memory mem, int ncores) {
	Processor *p = new Processor();
	p->coreMemory = new Memory[ncores];
	p->coreMemory[0] = mem;
	for (int i = 1; i < ncores; ++i) {
		p->coreMemory[i] = Memory::duplicateMemory(mem);
	}
	p->ncores = ncores;
	return *p;
}

Memory::Memory(){
}

 /* Arquivo baseado no trabalho de um grupo da turma. Obrigado ao grupo.
 
int main(int argv, char* args)
{

    // variaveis necessarias para a execu�ao do arquivo
    // SACache l1d, l1i, l2, l3;
    // MainMemory mp;

    // incluir variaveis que registrem o resultado de cada comando

    ifstream arq(args[1]);
    string comando;
    arq >> comando;

    while(comando != ""){
        switch(comando) {
            case "cl1d":
                int c, a, l;
                arq >> c  >>  a >> l;
                //l1d = createSACache(c,a,l);
                //tratar erro, se ocorrer
                break;
            case "cl1i":
                int c, a, l;
                arq >> c  >>  a >> l;
                //l1i = createSACache(c,a,l);
                //tratar erro, se ocorrer
                break;
            case "cl2":
                int c, a, l;
                arq >> c  >>  a >> l;
                //l2 = createSACache(c,a,l);
                //tratar erro, se ocorrer
                break;
            case "cl3":
                int c, a, l;
                arq >> c  >>  a >> l;
                //l3 = createSACache(c,a,l);
                //tratar erro, se ocorrer
                break;
            case "cmp":
                int ramsize, vmsize;
                arq >> ramsize >> vmsize;
                //mp = createMainMemory(ramsize,vmsize);
                break;
        //... outros comandos
        }

        arq >> comando;
    }

    // imprimir o resultado da sequencia de comandos.
}
*/

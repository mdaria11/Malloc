    Tema 2 - Memory Allocator
    Moldovan Daria - 334CC

  In helpers.h am adaugat un macro ALIGN(size) intoarce cel mai aproape multiplu de 8
  de size-ul nostru mai mare sau egal decat size si SIZE_HEADER este size-ul headerului
  de metadate aligned.

  Malloc:

    Mai intai verificam daca size-ul este mai mare sau mai mic decat threshold; daca e mai mare
    alocam un bloc de memorie nou cu mmap, altfel verificam daca este cumva primul malloc din
    program (adica nu avem nimic alocat pe heap) si facem un prealloc de 128kb pe heap folosind
    sbrk (bloc free). Pentru alocarea in sine, cautam in lista de blocuri blocuri free si vedem
    care are size-ul mai mare si cat mai aproape de size-ul nostru (best find). Daca am gasit un 
    bloc, verificam daca putem sa facem split pe el, adica sa pastram un bloc de size-ul nostru
    ca alocat si restul sa devina un bloc nou free (putem sa facem split doar daca blocul nou 
    free are cel putin un payload de 1 byte), si returnam adresa payload-ului din bloc. Daca
    n-am gasit un bloc, atunci vedem daca ultimul bloc este free; daca este il extindem folosind
    sbrk si returnam adresa payloadului. Daca nu atunci alocam cu brk un bloc intreg cu cat ne
    trebuie noua.

Free:

    Daca blocul pe care vrem sa-l eliberam este mapped, atunci folosim munmap sa=l dezalocam.
    Daca blocul e alocat pe heap, atunci ii schimbam statusul in free si apelam functia de 
    coalesce ca sa ne asiguram ca nu exista blocuri free una dupa alta.

Calloc:

    Calloc este identic cu implementarea malloc-ului, singurele modificari sunt valoarea de
    threshold pentru mmap (este de 4kb aka page size), size-ul total al blocului o sa fie de
    nmem*size si blocul este initializat cu 0-uri folosind memset.

Realloc:

    Daca blocul initial este mapped, atunci apelam malloc pentru un bloc nou mapped
    sau pe heap in functie de size si copiem datele din blocul vechi in cel nou.
    Daca avem blocul initial pe heap; daca size nou este mai mic decat size vechi,
    atunci pastram blocul, doar verificam daca putem sa facem split pentru a face
    un bloc nou free. Daca size nou este mai mare decat size vechi: daca size e mai
    mare decat threshold atunci alocam cu malloc un bloc nou si copiem datele, daca
    e mai mic decat threshold facem coalesce si luam cele 3 cazuri: daca blocul este
    ultimul din heap, vedem daca exista un bloc free pentru size-ul nou si daca este
    copiem datele in blocul nou si facem split daca putem, daca nu gasim extindem 
    blocul; daca blocul nu este ultimul din heap iar urmatorul dupa el este free, 
    vedem daca unirea lor e suficienta pentru size-ul nostru, daca da le unim si facem
    split daca putem; daca nu e suficient facem alocare cu malloc; daca blocul
    nu este ultimul din heap si urmatorul bloc nu e free, alocam un bloc nou cu 
    malloc si copiem datele.
    
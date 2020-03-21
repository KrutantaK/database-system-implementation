#include "RelOp.h"
#include <iostream>

// ***** SELECT FILE ******* //

void SelectFile::Run (DBFile &inFile, Pipe &outPipe, CNF &selOp, Record &literal) {
    this->inFile = &inFile;
    this->outPipe = &outPipe;
    this->selOp = &selOp;
    this->literal = &literal;
    this->buffer = new Record;
    pthread_create(&thread, NULL, select_file_helper, (void*)this);
}

void SelectFile::WaitUntilDone () {
	pthread_join (thread, NULL);
}

void SelectFile::Use_n_Pages (int runlen) {
    // only one temp record being used (no pages)
    return;
}

void* SelectFile::select_file_helper (void* arg) {
    SelectFile *s = (SelectFile *)arg;
    s->select_file_function();
}

void* SelectFile::select_file_function () {
    cout << "starting select file\n";
    inFile->MoveFirst();
    int count = 0;
    while (inFile->GetNext(*buffer, *selOp, *literal)) {
        count++;
        outPipe->Insert(buffer);
    }
    cout << "select file count : " << count << "\n";

    outPipe->ShutDown();
    pthread_exit(NULL);
}

// ***** SELECT PIPE ******* //

void SelectPipe::Run (Pipe &inPipe, Pipe &outPipe, CNF &selOp, Record &literal) {
    this->inPipe = &inPipe;
    this->outPipe = &outPipe;
    this->selOp = &selOp;
    this->literal = &literal;
    this->buffer = new Record;
    pthread_create(&thread, NULL, select_pipe_helper, (void*)this);
}

void SelectPipe::WaitUntilDone () {
    pthread_join (thread, NULL);
}

void SelectPipe::Use_n_Pages (int runlen) {
    // only one temp record being used (no pages)
    return;
}

void* SelectPipe::select_pipe_helper (void* arg) {
    SelectPipe *s = (SelectPipe *)arg;
    s->select_pipe_function();
}

void* SelectPipe::select_pipe_function () {
    cout << "starting select pipe\n";
    ComparisonEngine ce;
    int count = 0;
    while (inPipe->Remove(buffer)) {
        if (ce.Compare(buffer, literal, selOp)) {
            count++;
            outPipe->Insert(buffer);
        }
    }

    cout << "select pipe count : " << count << "\n";
    outPipe->ShutDown();
    pthread_exit(NULL);
}

// ***** PROJECT ******* //

void Project::Run (Pipe &inPipe, Pipe &outPipe, int *keepMe, int numAttsInput, int numAttsOutput) {
    this->inPipe = &inPipe;
    this->outPipe = &outPipe;
    this->keepMe = keepMe;
    this->numAttsInput = numAttsInput;
    this->numAttsOutput = numAttsOutput;
    this->buffer = new Record;
    pthread_create(&thread, NULL, project_helper, (void*)this);
}

void Project::WaitUntilDone () {
    pthread_join (thread, NULL);
}

void Project::Use_n_Pages (int runlen) {
    // only one temp record being used (no pages)
    return;
}

void* Project::project_helper (void* arg) {
    Project *s = (Project *)arg;
    s->project_function();
}

void* Project::project_function () {
    cout << "starting project\n";
    int count = 0;
    while (inPipe->Remove(buffer)) {
        count++;
        buffer->Project(keepMe, numAttsOutput, numAttsInput);
        outPipe->Insert(buffer);
    }

    cout << "project count : " << count << "\n";
    outPipe->ShutDown();
    pthread_exit(NULL);
}

// ***** WRITE OUT ******* //

void WriteOut::Run (Pipe &inPipe, FILE *outFile, Schema &mySchema) {
    this->inPipe = &inPipe;
    this->outFile = outFile;
    this->mySchema = &mySchema;
    this->buffer = new Record;
    pthread_create(&thread, NULL, writeout_helper, (void*)this);
}

void WriteOut::WaitUntilDone () {
    pthread_join (thread, NULL);
}

void WriteOut::Use_n_Pages (int runlen) {
    // only one temp record being used (no pages)
    return;
}

void* WriteOut::writeout_helper (void* arg) {
    WriteOut *s = (WriteOut *)arg;
    s->writeout_function();
}

void* WriteOut::writeout_function () {
    cout << "starting write out\n";
    int n = mySchema->GetNumAtts();
    Attribute *attributes = mySchema->GetAtts();

    int count = 0;
    while (inPipe->Remove(buffer)) {
        for (int i = 0 ; i < n ; i++) {
            fprintf(outFile, "%s:[", attributes[i].name);
            int pos = ((int *) buffer->bits)[i+1];

            if (attributes[i].myType == Int) {
                int *myInt = (int *) &(buffer->bits[pos]);
                fprintf(outFile, "%d", *myInt);
            }
            else if (attributes[i].myType == Double) {
                double *myDouble = (double *) &(buffer->bits[pos]);
                fprintf(outFile, "%f", *myDouble);
            }
            else if (attributes[i].myType == String) {
                char *myString = (char *) &(buffer->bits[pos]);
                fprintf(outFile, "%s", myString);
            }
            fprintf(outFile, "%c", ']');
            if (i != n-1)
                fprintf(outFile, "%c", ',');
        }
        count++;
        fprintf(outFile, "%c", '\n');        
    }

    cout << "writeout count : " << count << "\n";
    fclose(outFile);
    pthread_exit(NULL);
}

// ***** DUPLICATE REMOVAL ******* //

void DuplicateRemoval::Run (Pipe &inPipe, Pipe &outPipe, Schema &mySchema) {
    this->inPipe = &inPipe;
    this->outPipe = &outPipe;
    this->mySchema = &mySchema;
    pthread_create(&thread, NULL, duplicate_helper, (void*)this);
}

void DuplicateRemoval::WaitUntilDone () {
    pthread_join (thread, NULL);
}

void DuplicateRemoval::Use_n_Pages (int runlen) {
    this->runlen = runlen;
    return;
}

void* DuplicateRemoval::duplicate_helper (void* arg) {
    DuplicateRemoval *s = (DuplicateRemoval *)arg;
    s->duplicate_function();
}

void* DuplicateRemoval::duplicate_function () {
    cout << "starting duplicate removal\n";
    OrderMaker sortorder(mySchema);
    Pipe *sortedOutPipe = new Pipe(100);
    BigQ sortQ(*inPipe, *sortedOutPipe, sortorder, runlen);

    Record prev, curr;
    ComparisonEngine ce;
    sortedOutPipe->Remove(&prev);

    int count = 0;
    while (sortedOutPipe->Remove(&curr)) {
        if (ce.Compare(&prev, &curr, &sortorder)) {
            count++;
            outPipe->Insert(&prev);
            prev.Consume(&curr);
        }         
    }
    outPipe->Insert(&prev);
    count++;

    cout << "after duplicate removal count : " << count << "\n";
    outPipe->ShutDown();
    pthread_exit(NULL);
}
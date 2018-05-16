#include "ofApp.h"

#include <sstream>

void drawNeuron(ofPoint P, float size) {
        ofSetColor(ofColor::white);
        ofDrawCircle(P, 5);
        ofSetColor(100, 40, 0);
        ofDrawCircle(P, 16*size);    
}


std::vector<float> listToNumbers(std::string list) {
    stringstream ss;
    ss.str(list);
    std::vector<float> numbers;
    for(float f = 0; ss >> f; numbers.push_back(f));
    return numbers;
}


void printNumbers(std::vector<float> list) {
    for(unsigned int i=0; i<list.size(); i++) {
        std::cout << list[i] << " ";
    } 
}

net::net(unsigned int n_ins, unsigned int n_outs) {
    for(unsigned int i=0; i<n_ins; i++)
        inputs.push_back(std::make_shared<float>(0.5) );
    
    for(unsigned int i=0; i<n_outs; i++)
        outputs.push_back(std::make_shared<float>(0) );
}


void net::forward() {
    unsigned int N = inputs.size();
    if(outputs.size() < N) N = outputs.size();
    
    for(unsigned int i=0; i<N; i++)
        *(outputs[i]) = *(inputs[i]);
}


std::vector<std::vector<int> > net::getLinks() {
    std::vector<std::vector<int > > result(outputs.size() );
    std::vector<int> kaikkiInputit(inputs.size() );
    for(unsigned int i=0; i<kaikkiInputit.size(); i++) 
        kaikkiInputit[i]=i;
    for(unsigned int i=0; i < result.size(); i++)
        result[i] = kaikkiInputit;
    return result;
}


std::vector<std::vector<float> > net::getWeights() {
    std::vector<std::vector<float> > result(outputs.size() );
    std::vector<float> kaikkiPainot(inputs.size() );
        
    for(unsigned int i=0; i < result.size(); i++) {
        for(unsigned int j=0; j<kaikkiPainot.size(); j++) 
            kaikkiPainot[j]= ((float)j/kaikkiPainot.size());
        
        result[i] = kaikkiPainot;
    }
    return result;
}


void ofApp::setInputValues(std::vector<float> values) {
    unsigned int N = inputs.size();
    if(values.size() < inputs.size() ) N = values.size();
    for(unsigned int i=0; i<N; i++)
        *inputs[i] = values[i];
}

void ofApp::setOutputValues(std::vector<float> values) {
    unsigned int N = outputs.size();
    if(values.size() < outputs.size() ) N = values.size();
    for(unsigned int i=0; i<N; i++)
        *outputs[i] = values[i];
}

void ofApp::receive() {
    //hakee paketin ja muuttaa inputs -pointtereiden arvoja

    //paketti voi olla:
    //1) inputs <f> <f> <f> <f> ...
    //2) learning <1|0>
    //3) desiredOutputs <f> <f> <f> <f> ...
    
    //vastaanotetaan paketti. Jos tyhjä, palataan
    char udpData[MAX_PACKET_SIZE];
    std::string packet_str;
    for(; udpReceiver.Receive(udpData, MAX_PACKET_SIZE); packet_str = udpData) {
        std::cout << "Tuli paketti: \"" << packet_str << "\"\n";
        if(packet_str == "") continue;

        //luetaan eka sana ja verrataan käskyihin
        stringstream ss;
        std::string word;
        ss.str(packet_str);
        ss >> word;
        
        if(word.empty() ) {
            std::cout << "Huono paketti: " << packet_str << "\n";
            continue;
        }

        if(word == "inputs") {
            std::vector<float> numbers;
            for(float f = 0; ss >> f; numbers.push_back(f));
            std::cout << "Asetetaan inputit: ";
            printNumbers(numbers);
            std::cout << "\n";
            if(numbers.empty() ) continue;
            setInputValues(numbers);
        }

        else if (word == "desiredOutputs") {
            std::vector<float> numbers;
            for(float f = 0; ss >> f; numbers.push_back(f));
            std::cout << "Asetetaan desiredOutputit: ";
            printNumbers(numbers);
            std::cout << "\n";
            if(numbers.empty() ) continue;
            desiredOutputs = numbers;
            learn = true;
        }

        else {
            std::cout << "Huono käsky: " << word << "\n";
            continue;
        }
    }
}


void ofApp::send() {
    //lähetetään luvut asciina
    stringstream ss;
    for(unsigned int i=0; i<outputs.size(); i++)  {
        ss << *outputs[i];
        if(i < outputs.size()-1)
            ss << " ";
        else
            ss << ";\n";
    }
    //std::cout << "Lähetetään: " << ss.str().c_str() << "\n";
    udpSender.Send(ss.str().c_str(), ss.str().length() );
}

//--------------------------------------------------------------
void ofApp::setup(){

    /* luodaan neural net */
    
    NN = NNet(N_outputs, N_hiddenLayers);
    
    //links from hidden layers and the output layer:
    links.resize(N_hiddenLayers + 1);
    
    inputs.resize(N_inputs);
    for(unsigned int i=0; i<N_inputs; i++)
        inputs[i] = std::make_shared<float>(0);
    
    //get shared pointers to net's outputs
    outputs = NN.getOutputSignals();
    
    NN.setLearningcurve(0.98, 0.1, 0.5);

    NN.linkInput(inputs);
    
    //link layers to previous layers. Return value is a vector< vector<int> > which represents all links in a layer
    for(unsigned int i=0; i < N_hiddenLayers; i++) {
        if(i == 0)
            links[i] = NN.linkHidden(i+1, hiddenLayerSize);
        else
            links[i] = NN.linkHidden(i+1, ALL_COMBINATIONS);
        
        std::cout << "hidden layer " << i+1 << ": " << links[i].size() << " neurons\n";
    }
    
    links[links.size()-1] = NN.linkOutput();
    
    std::cout << "output layer: " << links[links.size()-1].size() << " neurons\n";
    std::cout << links[links.size()-1][0].size() << " links\n";
    
    
    //NN.getStats(); //kertoo juttuja
    
    /* luodaan UDP-yhteydet */

    udpSender.Create();
    udpSender.Connect(SEND_IP, SEND_PORT);
    udpSender.SetNonBlocking(true);
    
    udpReceiver.Create();
    udpReceiver.Bind(RECEIVE_PORT);
    udpReceiver.SetNonBlocking(true);
    
    /* piirron asetuksia */
    
    ofSetBackgroundColor(ofColor::white);
    ofSetColor(100, 40, 0);
    
    ofSetFrameRate(20);
}

//--------------------------------------------------------------
void ofApp::update(){    

    receive();
    
    if(learn) {
        NN.back(desiredOutputs);
        learn = false;
    }
    
    NN.forward();
    
    send();
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    unsigned int N_layers = N_hiddenLayers + 2;
    
    float margin = 20;
    float row_h = (ofGetHeight() - margin*3) / (N_layers-1);
    float spacing;
    float c;
    
    
    std::vector<ofPoint> previousLayerPoints;
    std::vector<ofPoint> thisLayerPoints;
    std::vector<std::vector<float> > layerWeights;
    //inputs
    spacing = (ofGetWidth() - margin*2) / (inputs.size()+1);
    for(unsigned int i=0; i<inputs.size(); i++) {
        thisLayerPoints.push_back(ofPoint( (i+1) * spacing + margin, margin) );
        c = 0.5;
        drawNeuron(thisLayerPoints.back(), c);
    }
    //hidden layers & output layer
    for(unsigned int layer_i=0; layer_i < links.size(); layer_i++) {
        
        std::cout << "get weights\n";
        layerWeights = NN.getWeights(layer_i+1);
        std::cout << "layer:" << layer_i+1 << ", neurons:" << layerWeights.size() << ", weights:" << layerWeights[0].size() << "\n";

        unsigned int layerSize = links[layer_i].size();
        
        previousLayerPoints = thisLayerPoints;
        thisLayerPoints.resize(layerSize);

        spacing = (ofGetWidth() - margin*2) / (layerSize+1);
        
        for(unsigned int neuron_i=0; neuron_i < layerSize; neuron_i++) {
            if(layerSize > 1)
                thisLayerPoints[neuron_i] = (ofPoint(neuron_i * spacing + margin + spacing, (layer_i+1) * row_h + margin) );
            else
                thisLayerPoints[neuron_i] = (ofPoint(ofGetWidth()/2, (layer_i+1) * row_h + margin) );
        }
            
        
        for(unsigned int neuron_i=0; neuron_i < layerSize; neuron_i++) {
            std::vector<int> neuronLinks = links[layer_i][neuron_i];
            
            //draw links
            for(unsigned int link_i = 0; link_i < neuronLinks.size(); link_i++) {
                ofPoint linkStart = thisLayerPoints[neuron_i];
                ofPoint linkEnd = previousLayerPoints[neuronLinks[link_i] ];
                c = 5 * layerWeights[neuron_i][link_i+1]; //bias on layerWeights[...][0]
                ofSetLineWidth(c);
                ofDrawLine(linkStart, linkEnd);
            }
            
            //draw point
            c = 0.5; //TODO: tähän output-arvot
            drawNeuron(thisLayerPoints[neuron_i], c);
            
        }
        std::cout << "ok\n";
    }

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
// neural-net-tutorial.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <fstream>

using namespace std;

struct Connection
{
	double weight;
	double deltaWeight;
};

class Neuron;
typedef vector<Neuron> Layer;

// ****************** class Neuron ****************

class Neuron 
{
public:
	Neuron(unsigned numOutputs, unsigned myIndex);
	void setOutputVal(double val) { m_outputVal = val;  }
	double getOutputVal(void) const { return m_outputVal;  }
	void feedForward(const Layer &prevLayer);
	void calcOutputGradients(double targetVal);
	void calcHiddenGradients(const Layer &nextLayer);
	void updateInputWeights(Layer &prevLayer);

private:
	static double eta; // [0.0 .. 1.0] overall net training rate
	static double alpha; // [0.0 .. n] multiplier of last weight change (momentum)
	static double transferFunction(double x);
	static double transferFunctionDerivative(double x);
	static double randomWeight(void) { return rand() / double(RAND_MAX); }
	double sumDOW(const Layer &nextLayer) const;
	double m_outputVal;
	vector<Connection> m_outputWeights;
	unsigned m_myIndex;
	double m_gradient;
};

double Neuron::eta = 0.15;
double Neuron::alpha = 0.5;

void Neuron::updateInputWeights(Layer &prevLayer)
{
	// The weights to be updated are in the Connection container
	// in the neurons in the preceding layer

	for (unsigned n = 0; n < prevLayer.size(); ++n) {
		Neuron &neuron = prevLayer[n];
		double oldDeltaWeight = neuron.m_outputWeights[m_myIndex].deltaWeight;
		double newDeltaWeight =
			// Individual input, magnified by the gradient and train rate:
			eta
			* neuron.getOutputVal()
			* m_gradient
			// Also add momentum = a fraction of the previous delta weight
			+ alpha
			* oldDeltaWeight;

		neuron.m_outputWeights[m_myIndex].deltaWeight = newDeltaWeight;
		neuron.m_outputWeights[m_myIndex].weight += newDeltaWeight;
	}
}
double Neuron::sumDOW(const Layer &nextLayer) const
{
	double sum = 0.0;

	// Sum our contributions of the errors at the nodes we feed

	for (unsigned n = 0; n < nextLayer.size() - 1; ++n) {
		sum += m_outputWeights[n].weight * nextLayer[n].m_gradient;
	}

	return sum;
}
void Neuron::calcOutputGradients(double targetVal)
{
	double delta = targetVal - m_outputVal;
	m_gradient = delta * Neuron::transferFunctionDerivative(m_outputVal);
}

void Neuron::calcHiddenGradients(const Layer &nextLayer)
{
	double dow = sumDOW(nextLayer);
	m_gradient = dow * Neuron::transferFunctionDerivative(m_outputVal);
}

double Neuron::transferFunction(double x)
{
	// tanh - output range [-1.0 .. 1.0]
	return tanh(x);
}

double Neuron::transferFunctionDerivative(double x)
{
	// tanh - output range [-1.0 .. 1.0]
	return 1 - x*x;
}

void Neuron::feedForward(const Layer &prevLayer)
{
	double sum = 0;

	for (const auto &neuron : prevLayer)
	{
		sum += neuron.getOutputVal() * neuron.m_outputWeights[m_myIndex].weight;
	}

	m_outputVal = Neuron::transferFunction(sum);
}

Neuron::Neuron(unsigned numOutputs, unsigned myIndex)
{
	for (unsigned c = 0; c < numOutputs; ++c) {
		m_outputWeights.push_back(Connection());
		m_outputWeights.back().weight = randomWeight();
	}

	m_myIndex = myIndex;
}

// ****************** class Net ****************
class Net
{
public:
	Net(const vector<unsigned> &topology);
	void feedForward(const vector<double> &inputVals);
	void backProp(const vector<double> &targetVals);
	void getResults(vector<double> &resultVals) const;

private:
	vector<Layer> m_layers; // m_layers[layerNum][neuronNum]
	double m_error;
	double m_recentAverageError;
	double m_recentAverageSmoothingFactor;
};

void Net::getResults(vector<double> &resultVals) const
{
	resultVals.clear();

	for (unsigned n = 0; n < m_layers.back().size() - 1; ++n)
	{
		resultVals.push_back(m_layers.back()[n].getOutputVal());
	}
}
void Net::backProp(const vector<double> &targetVals)
{
	// RMS - root mean square errror
	Layer& outputLayer = m_layers.back();
	m_error = 0.0;

	for (unsigned n = 0; n < outputLayer.size() - 1; ++n)
	{
		double delta = targetVals[n] - outputLayer[n].getOutputVal();
		m_error += delta * delta;
	}
	m_error /= outputLayer.size() - 1;
	m_error = sqrt(m_error);

	// Implement a recent average measurement:
	m_recentAverageError = (m_recentAverageError * m_recentAverageSmoothingFactor + m_error) / (m_recentAverageSmoothingFactor + 1.0);

	// Calculate output layer gradients
	
	for (unsigned n = 0; n < outputLayer.size() - 1; ++n)
	{
		outputLayer[n].calcOutputGradients(targetVals[n]);
	}

	// Calculate hidden layer gradients

	for (unsigned layerNum = m_layers.size() - 2; layerNum > 0; --layerNum)
	{
		Layer &hiddenLayer = m_layers[layerNum];
		Layer &nextLayer = m_layers[layerNum + 1];

		for (unsigned n = 0; n < hiddenLayer.size(); ++n) {
			hiddenLayer[n].calcHiddenGradients(nextLayer);
		}
	}

	// For all layers from outputs to first hidden layer,
	// update connection weights

	for (unsigned layerNum = m_layers.size() - 1; layerNum > 0; --layerNum)
	{
		Layer &layer = m_layers[layerNum];
		Layer &prevLayer = m_layers[layerNum - 1];

		for (unsigned n = 0; n < layer.size() - 1; ++n)
		{
			layer[n].updateInputWeights(prevLayer);
		}
	}
}
void Net::feedForward(const vector<double> &inputVals)
{
	// Number of input values MUST = number of input neurons
	assert(inputVals.size() == m_layers[0].size()-1);

	// Assign (latch) the input values into the input neurons
	for (unsigned i = 0; i < inputVals.size(); ++i)
	{
		m_layers[0][i].setOutputVal(inputVals[i]);
	}

	// Forward propagate
	for (unsigned layerNum = 1; layerNum < m_layers.size(); ++layerNum)
	{
		// Create references, fast!
		Layer &prevLayer = m_layers[layerNum - 1];
		for (unsigned n = 0; n < m_layers[layerNum].size() - 1; ++n)
		{
			m_layers[layerNum][n].feedForward(prevLayer);
		}
	}
}


Net::Net(const vector<unsigned> &topology)
{
	unsigned numLayers = topology.size();
	for (unsigned layerNum = 0; layerNum < numLayers; ++layerNum)
	{
		m_layers.push_back(Layer());
		unsigned numOutputs = layerNum == topology.size() - 1 ? 0 : topology[layerNum + 1];
		// We have made a new Layer, now fill it with neurons, and 
		// add a bias neuron to the layer:
		for (unsigned neuronNum = 0; neuronNum <= topology[layerNum]; ++neuronNum)
		{
			m_layers.back().push_back(Neuron(numOutputs, neuronNum)); // last element in vector
			cout << "made a neuron\n";
		}

		// Force bias neuron to output 1
		m_layers.back().back().setOutputVal(1.0);
	}

}
int main()
{
	ifstream fin("../training.txt");

	vector<unsigned> topology;
	unsigned topologySize;
	fin >> topologySize;

	for (unsigned i = 0; i < topologySize; i++)
	{
		unsigned layerSize;
		fin >> layerSize;
		topology.push_back(layerSize);
	}

	Net myNet(topology);

	unsigned trainingSets;
	fin >> trainingSets;

	unsigned inputSize = topology.front();
	unsigned outputSize = topology.back();
	for (unsigned i = 0; i < trainingSets; i++)
	{
		vector<double> inputVals;
		for (unsigned j = 0; j < inputSize; j++)
		{
			double x;
			fin >> x;
			inputVals.push_back(x);
		}

		vector<double> outputVals;
		for (unsigned j = 0; j < outputSize; j++)
		{
			double x;
			fin >> x;
			outputVals.push_back(x);
		}

		myNet.feedForward(inputVals);

		myNet.backProp(outputVals);

		//vector<double> resultVals;

		//myNet.getResults(resultVals);

		//cout << "\nInput:\n";
		//for (auto x : inputVals)
		//	cout << x << " ";

		//cout << "\nResult:\n";
		//for (auto x : resultVals)
		//	cout << x << " ";

		//cout << "\nExpected:\n";
		//for (auto x : outputVals)
		//	cout << x << " ";

	}

	while (1)
	{
		cout << "input number: ";
		double x;
		cin >> x;
		myNet.feedForward(vector<double>{ x });
		vector<double> results;
		myNet.getResults(results);

		if (results.front() > 0)
			cout << "Positive\n";
		else
			cout << "Negative\n";

		//cout << results[0] << "\n";
		cout << "\n";
	}

	return 0;
}


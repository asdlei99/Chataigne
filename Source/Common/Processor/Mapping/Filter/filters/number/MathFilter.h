/*
  ==============================================================================

    MathFilter.h
    Created: 4 Jul 2018 2:15:50pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "../../MappingFilter.h"


class MathFilter :
	public MappingFilter
{
public:
	MathFilter(var params, Multiplex* multiplex);
	~MathFilter();

	enum Operation { OFFSET, MULTIPLY, DIVIDE, MODULO, FLOOR, CEIL, ROUND, MAX, MIN };
	enum RangeRemapMode { KEEP, AJDUST, FREE };
	EnumParameter * operation;
	Parameter * operationValue;

	EnumParameter* rangeRemapMode;
	
	var opValueData; //for loading after setupParamInternal

	void setupParametersInternal(int multiplexIndex) override;
	bool processSingleParameterInternal(Parameter* source, Parameter* out, int multiplexIndex) override;

	void updateFilteredParamsRange();
	void filterParamChanged(Parameter * p) override;

	float getProcessedValue(float val, int index = -1);

	bool filteredParamShouldHaveRange();

	var getJSONData() override;
	void loadJSONDataInternal(var data) override;

	virtual String getTypeString() const override { return "Math"; }

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MathFilter)
};
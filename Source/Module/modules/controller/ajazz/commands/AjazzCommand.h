/*
  ==============================================================================

    AjazzCommand.h
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#pragma once

class AjazzModule;

class AjazzCommand :
	public BaseCommand
{
public:
	enum AjazzAction { 
        SET_COLOR, SET_IMAGE, SET_ALL_COLOR, SET_BRIGHTNESS, SET_TEXT, CLEAR_TEXTS,
        SET_SIDE_COLOR, SET_SIDE_IMAGE, SET_SIDE_TEXT
    };

	AjazzCommand(AjazzModule* _module, CommandContext context, var params, Multiplex* multiplex = nullptr);
	~AjazzCommand();

	AjazzAction action;
	AjazzModule* ajazzModule;

	IntParameter* row;
	IntParameter* column;
    IntParameter* sideIndex;
	Parameter * valueParam;

	void triggerInternal(int multiplexIndex) override;

	static BaseCommand* create(ControllableContainer* module, CommandContext context, var params, Multiplex * multiplex) { return new AjazzCommand((AjazzModule*)module, context, params, multiplex); }
};

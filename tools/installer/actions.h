/* ipl/installer/actions.h -*- C++ -*- Copyright (c) 2006 Joshua Oreman
 * Distributed under the GPL. See COPYING for details.
 */

#ifndef ACTIONS_H
#define ACTIONS_H

class ActionOutlet 
{
public:
    virtual void setTaskDescription (QString str) = 0;
    virtual void setCurrentAction (QString str) = 0;
    virtual void setTotalProgress (int tp) = 0;
    virtual void setCurrentProgress (int cp) = 0;
};

class Action 
{
public:
    Action (ActionOutlet *out)
        : _outlet (out)
    {}
    
    virtual void perform() = 0;
    
private:
    ActionOutlet *_outlet;
};

#endif

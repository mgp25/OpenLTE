/*******************************************************************************

    Copyright 2014 Ben Wojtowicz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

    File: LTE_fdd_enb_timer.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 timer class.

    Revision History
    ----------    -------------    --------------------------------------------
    05/04/2014    Ben Wojtowicz    Created file
    06/15/2014    Ben Wojtowicz    Added millisecond resolution and seperated
                                   the callback calling functionality.
    11/29/2014    Ben Wojtowicz    Added timer reset support.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_TIMER_H__
#define __LTE_FDD_ENB_TIMER_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

// Timer callback
class LTE_fdd_enb_timer_cb
{
public:
    typedef void (*FuncType)(void*, uint32);
    LTE_fdd_enb_timer_cb();
    LTE_fdd_enb_timer_cb(FuncType f, void* o);
    void operator()(uint32 id);
private:
    FuncType  func;
    void     *obj;
};
template<class class_type, void (class_type::*Func)(uint32)>
    void LTE_fdd_enb_timer_cb_wrapper(void *o, uint32 id)
{
    return (static_cast<class_type*>(o)->*Func)(id);
}

class LTE_fdd_enb_timer
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_timer(uint32 m_seconds, uint32 _id, LTE_fdd_enb_timer_cb _cb);
    ~LTE_fdd_enb_timer();

    // External interface
    void reset(void);
    void increment(void);
    bool expired(void);
    void call_callback(void);

private:
    // Identity
    LTE_fdd_enb_timer_cb cb;
    uint32               id;
    uint32               expiry_m_seconds;
    uint32               current_m_seconds;
};

#endif /* __LTE_FDD_ENB_TIMER_H__ */

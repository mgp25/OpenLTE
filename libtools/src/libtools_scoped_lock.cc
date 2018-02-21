/*******************************************************************************

    Copyright 2015 Ben Wojtowicz

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

    File: libtools_scoped_lock.cc

    Description: Contains all the implementations for the RAII class for sem_t
                 and pthread_mutex_t.

    Revision History
    ----------    -------------    --------------------------------------------
    12/05/2015    Ben Wojtowicz    Created file

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "libtools_scoped_lock.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/


/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

// Constructor/Destructor
libtools_scoped_lock::libtools_scoped_lock(sem_t &sem)
{
    sem_ = &sem;
    use_sem = true;
    sem_wait(sem_);
}
libtools_scoped_lock::libtools_scoped_lock(pthread_mutex_t &mutex)
{
    mutex_ = &mutex;
    use_sem = false;
    pthread_mutex_lock(mutex_);
}
libtools_scoped_lock::~libtools_scoped_lock()
{
    if(use_sem)
    {
        sem_post(sem_);
    }else{
        pthread_mutex_unlock(mutex_);
    }
}

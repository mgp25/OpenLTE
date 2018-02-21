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

    File: libtools_scoped_lock.h

    Description: Contains all the definitions for the RAII class for sem_t and
                 pthread_mutex_t.

    Revision History
    ----------    -------------    --------------------------------------------
    12/05/2015    Ben Wojtowicz    Created file

*******************************************************************************/

#ifndef __LIBTOOLS_SCOPED_LOCK_H__
#define __LIBTOOLS_SCOPED_LOCK_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include <semaphore.h>
#include <pthread.h>

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

class libtools_scoped_lock
{
public:
    libtools_scoped_lock(sem_t &sem);
    libtools_scoped_lock(pthread_mutex_t &mutex);
    ~libtools_scoped_lock();

private:
    sem_t *sem_;
    pthread_mutex_t *mutex_;
    bool use_sem;
};

#endif /* __LIBTOOLS_SCOPED_LOCK_H__ */

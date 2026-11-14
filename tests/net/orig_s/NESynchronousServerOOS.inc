// NESynchronousServer.cpp at the round's start (dcca2257), lines 719-726, unmodified (ne_checks_test)
            if (this->mIsOOS) {
                this->BeforeRemovingPlayers();
                for (unsigned long i = 0; i < this->mOOSSlots.size(); i++)
                    this->RemovePlayerFromGameInProgress(this->mOOSSlots[i], true, true, false);
                this->mIsOOS = false;
                // (RemovePlayerFromGameInProgress has already counted each removed human out of mNumHumanPlayers)
                this->mOOSSlots.clear();
            }

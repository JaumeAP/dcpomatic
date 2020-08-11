class PlayerVideoPreparer
{
public:
	virtual void request (boost::shared_ptr<PlayerVideo> pv) = 0;
};


class CPUPlayerVideoPreparer : public PlayerVideoPreparer
{
public:
	void request (boost::shared_ptr<PlayerVideo> pv);
};



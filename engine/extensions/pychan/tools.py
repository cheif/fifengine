# coding: utf-8

### Functools ###

def applyOnlySuitable(func,**kwargs):
	"""
	This nifty little function takes another function and applies it to a dictionary of
	keyword arguments. If the supplied function does not expect one or more of the
	keyword arguments, these are silently discarded. The result of the application is returned.
	This is useful to pass information to callbacks without enforcing a particular signature.
	"""
	if hasattr(func,'im_func'):
		code = func.im_func.func_code
		varnames = code.co_varnames[1:code.co_argcount]#ditch bound instance
	else:
		code = func.func_code
		varnames = code.co_varnames[0:code.co_argcount]

	#http://docs.python.org/lib/inspect-types.html
	if code.co_flags & 8:
		return func(**kwargs)
	for name,value in kwargs.items():
		if name not in varnames:
			del kwargs[name]
	return func(**kwargs)

def callbackWithArguments(callback,*args,**kwargs):
	"""
	Curries a function with extra arguments to
	create a suitable callback.

	If you don't know what this means, don't worry.
	It is designed for the case where you need
	different buttons to execute basically the same code
	with different argumnets.

	Usage::
	  # The target callback
	  def printStuff(text):
	      print text
	  # Mapping the events
	  gui.mapEvents({
	      'buttonHello' : callbackWithArguments(printStuff,"Hello"),
	      'buttonBye' : callbackWithArguments(printStuff,"Adieu")
	  })
	"""
	def real_callback():
		callback(*args,**kwargs)
	return real_callback

def this_is_deprecated(func,message=None):
	if message is None:
		message = repr(func)
	def wrapped_func(*args,**kwargs):
		print "PyChan: You are using the DEPRECATED functionality: %s" % message
		return func(*args,**kwargs)
	return wrapped_func

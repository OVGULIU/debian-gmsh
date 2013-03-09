import socket, struct, os, sys
_VERSION = '1.05'

class _parameter() :
  _membersbase = [
    ('name', 'string'), ('label', 'string', ''), ('help', 'string', ''),
    ('neverChanged', 'int', 0), ('changed', 'int', 1), ('visible', 'int', 1),
    ('read_only', 'int', 0), ('attributes', ('dict', 'string', 'string'), {}),
    ('clients', ('list', 'string'), [])
  ]
  _members = {
    'string' : _membersbase + [
      ('value', 'string'), ('kind', 'string', 'generic'), ('choices', ('list', 'string'), [])
    ],
    'number' : _membersbase + [
      ('value', 'float'), ('min', 'float', -sys.float_info.max), ('max', 'float', sys.float_info.max),
      ('step', 'float', 0.), ('index', 'int', -1), ('choices', ('list', 'float'), []),
      ('labels', ('dict', 'float', 'string'), {})
    ]
  }

  def __init__(self, type, **values) :
    self.type = type
    for i in _parameter._members[self.type] :
      setattr(self, i[0], values[i[0]] if i[0] in values else i[2])

  def tochar(self) :
    def tocharitem(l, t, v) :
      if t=='string' : l.append(v)
      elif t =='int': l.append(str(v))
      elif t=='float' : l.append('%.16g' % v)
      elif t[0]=='list' : 
        l.append(str(len(v)))
        for i in v : tocharitem(l, t[1], i)
      elif t[0]=='dict' :
        l.append(str(len(v)))
        for i, j in v.items() :
          tocharitem(l, t[1], i)
          tocharitem(l, t[2], j)
    msg = [_VERSION, self.type]
    for i in _parameter._members[self.type] :
      tocharitem(msg, i[1], getattr(self, i[0]))
    return '\0'.join(msg)

  def fromchar(self, msg) :
    def fromcharitem(l, t) :
      if t=='string' : return l.pop()
      elif t =='int': return int(l.pop())
      elif t=='float' : return float(l.pop())
      elif t[0]=='list' : return [fromcharitem(l, t[1]) for i in range(int(l.pop()))]
      elif t[0]=='dict' : return dict([(fromcharitem(l, t[1]),fromcharitem(l, t[2])) for i in range(int(l.pop()))])
    l = msg.split('\0')
    l.reverse()
    if l.pop() != _VERSION :
      print('onelab version mismatch')
    if l.pop() != self.type :
      print('onelab parameter type mismatch')
    for p in  _parameter._members[self.type]:
      setattr(self, p[0], fromcharitem(l, p[1]))
    
class client :
  _GMSH_START = 1
  _GMSH_STOP = 2
  _GMSH_INFO = 10
  _GMSH_MERGE_FILE = 20
  _GMSH_PARSE_STRING = 21
  _GMSH_PARAMETER = 23
  _GMSH_PARAMETER_QUERY = 24
  _GMSH_CONNECT = 27

  def _receive(self) :
    def buffered_receive(l) :
      msg = b''
      while len(msg) < l:
        chunk = self.socket.recv(l - len(msg))
        if not chunk :
          RuntimeError('onelab socket closed')
        msg += chunk
      return msg
    msg = buffered_receive(struct.calcsize('ii'))
    t, l = struct.unpack('ii', msg)
    msg = buffered_receive(l).decode('utf-8')
    if t == self._GMSH_INFO :
      print('onelab info : %s' % msg)
    return t, msg

  def _send(self, t, msg) :
    m = msg.encode('utf-8')
    if self.socket.send(struct.pack('ii%is' %len(m), t, len(m), m)) == 0 :
      RuntimeError('onelab socket closed')

  def _get_parameter(self, param) :
    if not self.socket :
      return
    self._send(self._GMSH_PARAMETER_QUERY, param.tochar())
    (t, msg) = self._receive() 
    if t == self._GMSH_PARAMETER :
      param.fromchar(msg)
    elif t == self._GMSH_INFO :
      self._send(self._GMSH_PARAMETER, param.tochar())

  def get_string(self, name, value, **param):
    param = _parameter('string', name=name, value=value, **param)
    self._get_parameter(param)
    return param.value

  def get_number(self, name, value, **param):
    if "labels" in param :
      param["choices"] = param["labels"].keys()
    p = _parameter('number', name=name, value=value, **param)
    self._get_parameter(p)
    return p.value

  def sub_client(self, name, command):
    self._send(self._GMSH_CONNECT, name)
    (t, msg) = self._receive()
    print ("python receive : ", t, msg)
    if t == self._GMSH_CONNECT and msg :
      print "python launch : "+ command + " -onelab "+ msg
      os.system(command + " -onelab " + name + " " + msg)

  def merge_file(self, filename) :
    if not self.socket :
      return
    if filename and filename[0] != '/' :
      filename = os.getcwd() + "/" + filename;
    self._send(self._GMSH_PARSE_STRING, 'Merge "'+filename+'";')

  def __init__(self):
    self.socket = None
    self.name = ""
    for i, v in enumerate(sys.argv) :
      if v == '-onelab':
        self.name = sys.argv[i + 1]
        addr = sys.argv[2]
        if '/' in addr or '\\' in addr or ':' not in addr :
          self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        else :
          self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect(sys.argv[i + 2])
        self._send(self._GMSH_START, str(os.getpid))
    self.action = self.get_string(self.name + '/Action', 'compute')
    if self.action == "initialize": exit(0)
  
  def __del__(self) :
    if self.socket :
      self._send(self._GMSH_STOP, 'Goodbye!')
      self.socket.close()

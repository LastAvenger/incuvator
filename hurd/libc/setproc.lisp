
(in-package :hurd)

(defcfun ("setproc" %setproc)
  :int
  (proc process-t))

(defun setproc (proc)
  (declare (type fixnum proc))
  (let ((err (%setproc proc)))
    (not (= err -1))))

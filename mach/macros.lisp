
(in-package :mach)

(defun %get-nil-task (task)
  "Returns (task-self) if 'task' is nil, 'task' otherwise."
  (if (null task)
    '(task-self)
    task))

(defun with-port-generic (destroy port-name creation task
                                  body)
  "Generates for code for port using and releasing."
  `(let ((,port-name ,creation))
     (when (port-valid ,port-name)
       (with-cleanup ,(funcall destroy port-name (%get-nil-task task))
         ,@body))))

(defmacro with-port-deallocate ((port-name creation &optional (task nil))
				&body body)
  "Uses a port and then port-deallocate's"
  (with-port-generic (lambda (port task)
                       `(port-deallocate ,port ,task))
                     port-name creation task body))

(defmacro with-port-destroy ((port-name creation &optional (task nil))
			     &body body)
  "Uses a port and then port-destroy's"
  (with-port-generic (lambda (port task)
                       `(port-destroy ,port ,task))
                     port-name creation task body))

(defun with-right-generic (what right body)
  "Wraps some code and then releases a port right"
  `(when (port-valid ,right)
     (with-cleanup (port-mod-refs ,right ,what -1)
		   ,@body)))

(defmacro with-receive-right (right &body body)
  "Wraps some code and then releases a receive right"
  (with-right-generic :right-receive
                      right
                      body))

(defmacro with-send-right (right &body body)
  "Wraps some code and then releases a send right"
  (with-right-generic :right-send
                      right
                      body))

(defun %generate-release-list (port task ls)
  "Generate code for port and port rights releasing based on the 'ls' list."
  (loop for right in (cadr ls)
        collect (if (eq right :deallocate)
                  `(port-deallocate ,port ,task)
                  `(port-mod-refs ,port ,right -1 ,task))))

(defmacro with-port ((port-name creation &optional (task nil))
                     release-list &body body)
  "Generic with-port, uses a port initialized with 'creation'
and then releases rights.
release-list can have :deallocate or any port right specified to port-mod-rights."
  (with-port-generic (lambda (port task)
                       `(progn ,@(%generate-release-list port task release-list)))
                     port-name creation task body))
